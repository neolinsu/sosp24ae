/*
 * softirq.c - handles backend processing (I/O, timers, ingress packets, etc.)
 */

#include <base/stddef.h>
#include <base/log.h>
#include <base/lock.h>
#include <runtime/thread.h>

#include "defs.h"
#include "net/defs.h"
#include "cxl/tqueue.h"


static bool softirq_iokernel_pending(struct kthread *k)
{
	return !lrpc_empty(&k->rxq);
}

static bool softirq_directpath_pending(struct kthread *k)
{
	return rx_pending(k->directpath_rxq);
}

static bool softirq_cxltp_pending(struct kthread *k)
{
#ifdef VESSEL_CXLTP
	return cfg_cxltp_enabled && tq_has_req(&k->cxltp_rx.tq);
#else
	return false;
#endif
}

static bool softirq_timer_pending(struct kthread *k, uint64_t now_tsc)
{
	uint64_t now_us = (now_tsc - start_tsc) / cycles_per_us;

	return ACCESS_ONCE(k->timern) > 0 &&
	       ACCESS_ONCE(k->timers[0].deadline_us) <= now_us;
}


static bool softirq_storage_pending(struct kthread *k)
{
	return storage_available_completions(&k->storage_q);
}

/**
 * softirq_pending - is there a softirq pending?
 */
bool softirq_pending(struct kthread *k, uint64_t now_tsc)
{
	bool res= softirq_iokernel_pending(k) ||
				softirq_directpath_pending(k) || 
				softirq_cxltp_pending(k) ||
				softirq_storage_pending(k) ||
				softirq_timer_pending(k, now_tsc) ;
	return res;
	// return softirq_timer_pending(k); // || softirq_cmp_pending(k);
}


/**
 * softirq_sched - schedule softirq work in scheduler context
 * @k: the kthread to check for softirq work
 *
 * The kthread's lock must be held when calling this function.
 *
 * Returns true if softirq work was marked ready.
 */
bool softirq_sched(struct kthread *k)
{
	uint64_t now_tsc = rdtsc();
	bool work_done = false;

	assert_preempt_disabled();
	assert_spin_lock_held(&k->lock);

	/* check for iokernel softirq work */
	if (!k->iokernel_busy && softirq_iokernel_pending(k)) {
		k->iokernel_busy = true;
		thread_ready_head_locked(k->iokernel_softirq);
		work_done = true;
	}

	/* check for directpath softirq work */
	if (!k->directpath_busy && softirq_directpath_pending(k)) {
	 	k->directpath_busy = true;
	 	thread_ready_head_locked(k->directpath_softirq);
	 	work_done = true;
	}

	/* check for cxltp softirq work */
	if (!k->cxltp_busy && softirq_cxltp_pending(k)) {
	 	k->cxltp_busy = true;
	 	thread_ready_head_locked(k->cxltp_softirq);
	 	work_done = true;
	}

	/* check for timer softirq work */
	if (!k->timer_busy && softirq_timer_pending(k, now_tsc)) {
		k->timer_busy = true;
		thread_ready_head_locked(k->timer_softirq);
		work_done = true;
	}

	/* check for storage softirq work */
	if (!k->storage_busy && softirq_storage_pending(k)) {
		k->storage_busy = true;
		thread_ready_head_locked(k->storage_softirq);
		work_done = true;
	}

	return work_done;
}

/**
 * softirq_steal - steal softirq work in scheduler context
 * @k: the kthread to check for softirq work
 *
 * The kthread's lock must be held when calling this function.
 *
 * Returns true if softirq work was marked ready.
 */
bool softirq_steal(struct kthread *k)
{
	uint64_t now_tsc = rdtsc();
	bool work_done = false;

	assert_preempt_disabled();
	assert_spin_lock_held(&k->lock);

	/* check for iokernel softirq work */
	if (!k->iokernel_busy && softirq_iokernel_pending(k)) {
		k->iokernel_busy = true;
		thread_ready_head_locked(k->iokernel_softirq);
		work_done = true;
	}

	/* check for directpath softirq work */
	if ((!k->directpath_busy && softirq_directpath_pending(k))
	) {
	 	k->directpath_busy = true;
	 	thread_ready_head_locked(k->directpath_softirq);
	 	work_done = true;
	}

	/* check for cxltp softirq work */
	if (!k->cxltp_busy && softirq_cxltp_pending(k)) {
	 	k->cxltp_busy = true;
	 	thread_ready_head_locked(k->cxltp_softirq);
	 	work_done = true;
	}

	/* check for timer softirq work */
	if (!k->timer_busy && softirq_timer_pending(k, now_tsc)) {
		k->timer_busy = true;
		thread_ready_head_locked(k->timer_softirq);
		work_done = true;
	}

	/* check for storage softirq work */
	if (!k->storage_busy && softirq_storage_pending(k)) {
		k->storage_busy = true;
		thread_ready_head_locked(k->storage_softirq);
		work_done = true;
	}

	return work_done;
}

/**
 * softirq_run - schedule softirq work in thread context
 * @k: the kthread to check for softirq work
 *
 * Returns true if softirq work was marked ready.
 */
bool softirq_run(void)
{
	struct kthread *k;
	uint64_t now_tsc = rdtsc();
	bool work_done = false;

	k = getk();
	if (!softirq_pending(k, now_tsc)) {
		putk();
		return false;
	}
	spin_lock(&k->lock);

	/* check for iokernel softirq work */
	if (!k->iokernel_busy && softirq_iokernel_pending(k)) {
		k->iokernel_busy = true;
		thread_ready_head_locked(k->iokernel_softirq);
		work_done = true;
	}

	/* check for directpath softirq work */
	if (!k->directpath_busy && softirq_directpath_pending(k)) {
	 	k->directpath_busy = true;
	 	thread_ready_head_locked(k->directpath_softirq);
	 	work_done = true;
	}

	/* check for directpath cxltp work */
	if (!k->cxltp_busy && softirq_cxltp_pending(k)) {
	 	k->cxltp_busy = true;
	 	thread_ready_head_locked(k->cxltp_softirq);
	 	work_done = true;
	}

	/* check for timer softirq work */
	if (!k->timer_busy && softirq_timer_pending(k, now_tsc)) {
		k->timer_busy = true;
		thread_ready_head_locked(k->timer_softirq);
		work_done = true;
	}
	// /* check for storage softirq work */
	// if (!k->storage_busy && softirq_storage_pending(k)) {
	// 	k->storage_busy = true;
	// 	thread_ready_head_locked(k->storage_softirq);
	// 	work_done = true;
	// }

	spin_unlock(&k->lock);
	putk();

	return work_done;
}


/**
 * lock_directpath - schedule softirq work in thread context
 * Returns true if softirq work was marked ready.
 */
void lock_directpath(void)
{
	struct kthread *k;

	k = getk();
	spin_lock(&k->lock);

	k->directpath_stealable = false;

	spin_unlock(&k->lock);
	putk();

	return;
}

/**
 * unlock_directpath - schedule softirq work in thread context
 * Returns true if softirq work was marked ready.
 */
void unlock_directpath(void)
{
	struct kthread *k;

	k = getk();
	spin_lock(&k->lock);

	k->directpath_stealable = true;

	spin_unlock(&k->lock);
	putk();

	return;
}

/**
 * softirq_do_directpath - schedule softirq work in thread context
 * Returns true if softirq work was marked ready.
 */
bool softirq_do_directpath(void)
{
	struct kthread *k;
	bool ret = true;

	k = getk();
	spin_lock(&k->lock);
	/* check for directpath softirq work */
	if (!k->directpath_busy && softirq_directpath_pending(k)) {
		k->directpath_busy = true;
		// log_info_ratelimited("do softirq");
		directpath_softirq_one(k);
		k->directpath_busy = false;
	}

	spin_unlock(&k->lock);
	putk();
	return ret;
}


/**
 * wait_directpath_busy - schedule softirq work in thread context
 * Returns true if softirq work was marked ready.
 */
void wait_directpath_busy(void)
{
	struct kthread*k = mykthread;
	while(true) {
		if (k->directpath_busy) {
			cpu_relax();
		} else {
			break;
		}
	}
	return;
}
