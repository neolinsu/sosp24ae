/*
 * init.c - initializes the runtime
 */
#include <pthread.h>
#include <stdio.h>

#include <vessel/interface.h>

#include <base/cpu.h>
#include <base/time.h>
#include <base/log.h>
#include <base/limits.h>
#include <runtime/thread.h>
#include <ctype.h>

#include "defs.h"

static pthread_barrier_t init_barrier;

int numa_nodeid;

struct init_entry {
	const char *name;
	int (*init)(void);
};

cpu_state_map_t cpu_state_map;
cpu_state_map_t *global_cpu_state_map=&cpu_state_map;
void __weak init_shutdown(int status)
{
	log_info("init: shutting down -> %s",
		 status == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
	exit(status);
}
bool core_init_done = false;
/**
 * ip_addr_to_str - prints an IP address as a human-readable string
 * @addr: the ip address
 * @str: a buffer to store the string
 *
 * The buffer must be IP_ADDR_STR_LEN in size.
 */
char *ip_addr_to_str(uint32_t addr, char *str)
{
	snprintf(str, IP_ADDR_STR_LEN, "%d.%d.%d.%d",
		 ((addr >> 24) & 0xff),
                 ((addr >> 16) & 0xff),
                 ((addr >> 8) & 0xff),
                 (addr & 0xff));
	return str;
}

static initializer_fn_t global_init_hook = NULL;
static initializer_fn_t perthread_init_hook = NULL;
static initializer_fn_t late_init_hook = NULL;


#define GLOBAL_INITIALIZER(name) \
	{__cstr(name), &name ## _init}

/* global subsystem initialization */
static const struct init_entry global_init_handlers[] = {
	/* runtime core */
	GLOBAL_INITIALIZER(minialloc),
	GLOBAL_INITIALIZER(page),
	GLOBAL_INITIALIZER(slab),
	GLOBAL_INITIALIZER(kthread),
	GLOBAL_INITIALIZER(ioqueues),
	GLOBAL_INITIALIZER(stack),
	GLOBAL_INITIALIZER(sched),
	GLOBAL_INITIALIZER(smalloc),
	GLOBAL_INITIALIZER(preempt),

	/* network stack */
	GLOBAL_INITIALIZER(net),
	GLOBAL_INITIALIZER(udp),
	GLOBAL_INITIALIZER(directpath),
	GLOBAL_INITIALIZER(arp),
	GLOBAL_INITIALIZER(trans),

	/* cxl */
	GLOBAL_INITIALIZER(cxltp),
	/* storage */
	GLOBAL_INITIALIZER(storage),

#ifdef GC
	GLOBAL_INITIALIZER(gc),
#endif
};

#define THREAD_INITIALIZER(name) \
	{__cstr(name), &name ## _init_thread}

/* per-kthread subsystem initialization */
static const struct init_entry thread_init_handlers[] = {
	/* runtime core */
	THREAD_INITIALIZER(thread),
	THREAD_INITIALIZER(page),
	THREAD_INITIALIZER(smalloc),
	THREAD_INITIALIZER(kthread),
	THREAD_INITIALIZER(ioqueues),
	THREAD_INITIALIZER(stack),
	THREAD_INITIALIZER(sched),
	THREAD_INITIALIZER(timer),
	THREAD_INITIALIZER(preempt),

	/* network stack */
	THREAD_INITIALIZER(net),
	THREAD_INITIALIZER(directpath),

	/* cxl */
	THREAD_INITIALIZER(cxltp),

	/* storage */
	THREAD_INITIALIZER(storage),
};

#define LATE_INITIALIZER(name) \
	{__cstr(name), &name ## _init_late}

static const struct init_entry late_init_handlers[] = {
	/* network stack */
	LATE_INITIALIZER(arp),
	LATE_INITIALIZER(stat),
	LATE_INITIALIZER(tcp),
	LATE_INITIALIZER(rcu),
	LATE_INITIALIZER(directpath),
};

static int run_init_handlers(const char *phase,
			     const struct init_entry *h, int nr)
{
	int i, ret;

	log_debug("entering '%s' init phase", phase);
	for (i = 0; i < nr; i++) {
		log_info("init -> %s", h[i].name);
		ret = h[i].init();
		if (ret) {
			return ret;
		}
	}
	log_init_done=true;
	return 0;
}

static int runtime_init_thread(void)
{

	int ret;
	ret = run_init_handlers("per-thread", thread_init_handlers,
				 ARRAY_SIZE(thread_init_handlers));

	if (ret || perthread_init_hook == NULL)
		return ret;
	return perthread_init_hook();

}

static void pthread_entry(void *data)
{
	int ret;
	ret = runtime_init_thread();

	BUG_ON(ret);
	pthread_barrier_wait(&init_barrier);
	// log_info("init: per-thread initialization done");
	sched_start();

	/* never reached unless things are broken */
	BUG();
	return;
}

/**
 * runtime_set_initializers - allow runtime to specifcy a function to run in
 * each stage of intialization (called before runtime_init).
 */
int runtime_set_initializers(initializer_fn_t global_fn,
			     initializer_fn_t perthread_fn,
			     initializer_fn_t late_fn)
{
	global_init_hook = global_fn;
	perthread_init_hook = perthread_fn;
	late_init_hook = late_fn;
	return 0;
}


/**
 * runtime_init - starts the runtime
 * @cfgpath: the path to the configuration file
 * @main_fn: the first function to run as a thread
 * @arg: an argument to @main_fn
 *
 * Does not return if successful, otherwise return  < 0 if an error.
 */
int runtime_init(const char *cfgpath, thread_fn_t main_fn, void *arg)
{
	int ret;
	log_debug("the configuration file is %s", cfgpath);
	log_debug("LD_PRELOAD is %s", getenv("LD_PRELOAD"));
	log_debug("LD_LIBRARY_PATH is %s", getenv("LD_LIBRARY_PATH"));
	ret = time_init();
	if (ret)
		return ret;
	core_init_done = true;

	ret = cfg_load(cfgpath);
	if (ret)
		return ret;
	
	cpu_info_tbl = *vessel_get_cpu_info();
	if (ret)
		return ret;

	pthread_barrier_init(&init_barrier, NULL, maxks);
	ret = run_init_handlers("global", global_init_handlers,
				ARRAY_SIZE(global_init_handlers));
	if (ret)
		return ret;

	if (global_init_hook) {
		ret = global_init_hook();
		if (ret) {
			log_err("User-specificed global initializer failed, ret = %d", ret);
			return ret;
		}
	}
	if(!bitmap_test( cpu_info_tbl.numa_mask, numa_nodeid)) {
		log_err("This runtime is not allowed to access NUMA: %d, try to change config!", numa_nodeid);
		abort();
	}
	ret = runtime_init_thread();

	BUG_ON(ret);
	
	vessel_cluster_spawn_all(pthread_entry, NULL);
	pthread_barrier_wait(&init_barrier);

	ret = ioqueues_register_iokernel();
	if (ret) {
		log_err("couldn't register with iokernel, ret = %d", ret);
		return ret;
	}

	/* point of no return starts here */
	ret = thread_spawn_main(main_fn, arg);
	BUG_ON(ret);

	ret = run_init_handlers("late", late_init_handlers,
				ARRAY_SIZE(late_init_handlers));
	BUG_ON(ret);

	if (late_init_hook) {
		ret = late_init_hook();
		if (ret) {
			log_err("User-specificed late initializer failed, ret = %d", ret);
			return ret;
		}
	}
	log_info("sched_start");
	sched_start();
	while(1);
	/* never reached unless things are broken */
	BUG();
	return 0;
}
