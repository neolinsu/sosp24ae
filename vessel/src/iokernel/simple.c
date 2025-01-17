/*
 * simple.c - a simple scheduler policy
 */

#include <stdlib.h>
#include <string.h>

#include <base/stddef.h>
#include <base/log.h>

#include "defs.h"
#include "sched.h"
#include "ksched.h"

/* a list of processes that are waiting for more cores */
static LIST_HEAD(congested_procs);
/* a bitmap of all available cores that are currently idle */
static DEFINE_BITMAP(simple_idle_cores, NCPU);
static bool is_server;

struct simple_data {
	struct proc		*p;
	unsigned int		is_congested:1;
	struct list_node	congested_link;
	uint64_t		qdelay_us;
	uint64_t		qdelay_tsc;

	/* thread usage limits */
	int			threads_guaranteed;
	int			threads_max;
	int			threads_active;

	/* congestion info */
	bool			waking;
};

static bool simple_proc_is_preemptible(struct simple_data *cursd,
				       struct simple_data *nextsd)
{
	return cursd->threads_active > cursd->threads_guaranteed &&
	       nextsd->threads_active < nextsd->threads_guaranteed;
}

/* the current process running on each core */
static struct simple_data *cores[NCPU];

/* the history of processes running on each core */
#define NHIST	4
static struct simple_data *hist[NCPU][NHIST];

static void simple_cleanup_core(unsigned int core)
{
	struct simple_data *sd = cores[core];
	int i;

	if (!sd)
		return;

	if (cores[core])
		cores[core]->threads_active--;
	cores[core] = NULL;
	for (i = NHIST-1; i > 0; i--)
		hist[core][i] = hist[core][i - 1];
	hist[core][0] = sd;
}

static void simple_mark_congested(struct simple_data *sd)
{
	if (sd->is_congested)
		return;
	sd->is_congested = true;
	list_add(&congested_procs, &sd->congested_link);
}

static void simple_unmark_congested(struct simple_data *sd)
{
	if (!sd->is_congested)
		return;
	sd->is_congested = false;
	list_del_from(&congested_procs, &sd->congested_link);
}

static int simple_attach(struct proc *p, struct sched_spec *cfg)
{
	struct simple_data *sd;

	/* TODO: validate if there are enough cores available for @cfg */

	sd = malloc(sizeof(*sd));
	if (!sd)
		return -ENOMEM;

	memset(sd, 0, sizeof(*sd));
	sd->p = p;
	sd->threads_guaranteed = cfg->guaranteed_cores;
	sd->threads_max = cfg->max_cores;
	sd->threads_active = 0;
	sd->waking = false;
	sd->qdelay_us = cfg->qdelay_us;
	sd->qdelay_tsc = cfg->qdelay_us * cycles_per_us;
	p->policy_data = (unsigned long)sd;
	return 0;
}

static void simple_detach(struct proc *p)
{
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	int i, j;

	simple_unmark_congested(sd);

	for (i = 0; i < NCPU; i++) {
		if (cores[i] == sd)
			cores[i] = NULL;
		for (j = 0; j < NHIST; j++) {
			if (hist[i][j] == sd)
				hist[i][j] = NULL;
		}
	}

	free(sd);
}

static int simple_run_kthread_on_core(struct proc *p, unsigned int core)
{
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	int ret;

	/*
	 * WARNING: A kthread could be stuck waiting to detach and thus
	 * temporarily unavailable even if it is no longer assigned to a core.
	 * We check with the scheduler layer here to catch such a race
	 * condition.  In this sense, applications can get new cores more
	 * quickly if they yield promptly when requested.
	 */
	if (sched_threads_avail(p) == 0)
		return -EBUSY;

	ret = sched_run_on_core(p, core);
	if (ret)
		return ret;

	simple_cleanup_core(core);
	cores[core] = sd;
	bitmap_clear(simple_idle_cores, core);
	sd->threads_active++;
	sd->waking = true;
	return 0;
}

static unsigned int simple_choose_core(struct proc *p)
{
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	struct thread *th;
	unsigned int core, tmp;

	/* first try to find a matching active hyperthread */
	sched_for_each_allowed_core(core, tmp) {
		unsigned int sib = sched_siblings[core];
		if (cores[core] != sd)
			continue;
		if (cores[sib] == sd || (cores[sib] != NULL &&
		    !simple_proc_is_preemptible(cores[sib], sd)))
			continue;
		if (bitmap_test(sched_allowed_cores, sib))
			return sib;
	}

	/* then try to find a previously used core (to improve locality) */
	list_for_each(&p->idle_threads, th, idle_link) {
		core = th->core;
		if (core >= NCPU)
			break;
		if (cores[core] != sd && (cores[core] == NULL ||
		    simple_proc_is_preemptible(cores[core], sd))) {
			return core;
		}

		/* sibling core has equally good locality */
		core = sched_siblings[th->core];
		if (cores[core] != sd && (cores[core] == NULL ||
		    simple_proc_is_preemptible(cores[core], sd))) {
			if (bitmap_test(sched_allowed_cores, core))
				return core;
		}
	}

	/* then look for any idle core */
	core = bitmap_find_next_set(simple_idle_cores, NCPU, 0);
	if (core != NCPU)
		return core;

	/* finally look for any preemptible core */
	sched_for_each_allowed_core(core, tmp) {
		if (cores[core] == sd)
			continue;
		if (cores[core] &&
		    simple_proc_is_preemptible(cores[core], sd))
			return core;
	}

	/* out of luck, couldn't find anything */
	return NCPU;
}

static int simple_add_kthread(struct proc *p)
{
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	unsigned int core;
	//printf("try to simple add kthread\n");
	if (sd->threads_active >= sd->threads_max)
		return -ENOENT;

	core = simple_choose_core(p);
	if (core == NCPU)
		return -ENOENT;

	return simple_run_kthread_on_core(p, core);
}

static int simple_notify_core_needed(struct proc *p)
{
	return simple_add_kthread(p);
}

static void set_proc_to_steal(struct proc *p, bool to_steal) {
#ifdef SERVER_KEEP_STEAL
	if (!is_server) {
		for (int i=0; i<p->thread_count; i++) {
			int core = p->threads[i].core;
			if (core != -1)
				ksched_set_to_steal(core, to_steal);
		}
	} else {
		for (int i=0; i<p->thread_count; i++) {
			int core = p->threads[i].core;
			if (core != -1)
				ksched_set_to_steal(core, true);
		}
		log_info_once("server keep steal");
	}
#else
	for (int i=0; i<p->thread_count; i++) {
		int core = p->threads[i].core;
		if (core != -1) {
			ksched_set_to_steal(core, to_steal);
		}
	}
#endif
}

static void simple_notify_congested(struct proc *p, bool busy, uint64_t delay, bool parked_thread_delay)
{
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	int ret;
	bool congested;

	/* detect congestion */
	congested = sd->qdelay_us == 0 ? busy : delay >= sd->qdelay_us;
	congested |= parked_thread_delay;
	/*if (congested) {
		log_info_ratelimited("parked_thread_delay: %d, qdelay: %lu, delay: %lu, busy: %d",
			parked_thread_delay,
			sd->qdelay_us,
			delay,
			busy
		);
	}*/
	set_proc_to_steal(p, congested);

	/* do nothing if we woke up a core during the last interval */
	if (sd->waking) {
		sd->waking = false;
		return;
	}

	/* check if congested */
	if (!congested) {
		simple_unmark_congested(sd);
		return;
	}

	/* do nothing if already marked as congested */
	if (sd->is_congested) {
		return;
	}

	/* try to add an additional core right away */
	ret = simple_add_kthread(p);
	if (ret == 0)
		return;

	/* otherwise mark the process as congested, cores can be added later */
	simple_mark_congested(sd);
}

void simple_notify_kthread_congested(struct proc *p, int kthread_id, uint64_t delay, bool hw_busy) {
	struct simple_data *sd = (struct simple_data *)p->policy_data;
	bool congested = sd->qdelay_tsc == 0 ? true : delay >= sd->qdelay_tsc;
	if (congested) {
		bool ret = lf_queue_put_with_meta(p->busy_kthreads, p->busy_kthreads_meta, kthread_id);
		if (ret) {
			log_info_ratelimited("qd: %ld, d:%ld", sd->qdelay_tsc, delay);
		}
	}
}


static struct simple_data *simple_choose_kthread(unsigned int core)
{
	struct simple_data *sd;
	int i;

	/* first try to run the same process as the sibling */
	sd = cores[sched_siblings[core]];
	if (sd && sd->is_congested && sched_threads_avail(sd->p)) {
		return sd;
	}

	/* then try to find a congested process that ran on this core last */
	for (i = 0; i < NHIST; i++) {
		sd = hist[core][i];
		if (sd && sd->is_congested && sched_threads_avail(sd->p)) {
			return sd;
		}

		/* the hyperthread sibling has equally good locality */
		sd = hist[sched_siblings[core]][i];
		if (sd && sd->is_congested && sched_threads_avail(sd->p))
			return sd;
	}

	/* then try to find any congested process */
	list_for_each(&congested_procs, sd, congested_link) {
		if (sched_threads_avail(sd->p)) {
			return sd;
		}
	}
	return NULL;
}

static void simple_sched_poll(uint64_t now, int idle_cnt, bitmap_ptr_t idle)
{
	struct simple_data *sd;
	unsigned int core;

	if (idle_cnt == 0) {
		return;
	}
	bitmap_for_each_set(idle, NCPU, core) {
		if (cores[core] != NULL)
			simple_unmark_congested(cores[core]);
		simple_cleanup_core(core);
		sd = simple_choose_kthread(core);
		if (!sd) {
			//puts("!sd");
			bitmap_set(simple_idle_cores, core);
			continue;
		}
		//puts("simple_run_kthread_on_core");
		if (unlikely(simple_run_kthread_on_core(sd->p, core))) {
			WARN();
			bitmap_set(simple_idle_cores, core);
			simple_mark_congested(sd);
		}
	}
}

struct sched_ops simple_ops = {
	.proc_attach		= simple_attach,
	.proc_detach		= simple_detach,
	.notify_congested	= simple_notify_congested,
	.notify_core_needed	= simple_notify_core_needed,
	.notify_kthread_congested = simple_notify_kthread_congested,
	.sched_poll		= simple_sched_poll
};

/**
 * simple_init - initializes the simple scheduler policy
 *
 * Returns 0 (always successful).
 */
int simple_init(void)
{
	char *vessel_name = getenv("VESSEL_NAME");
	if(strcmp("server", vessel_name) == 0) {
		is_server = true;
	} else if (strcmp("client", vessel_name) == 0) {
		is_server = false;
	} else {
		log_err("name err");
		abort();
	}
	bitmap_or(simple_idle_cores, simple_idle_cores,
		  sched_allowed_cores, NCPU);
	return 0;
}
