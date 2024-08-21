/*
 * ksched.h - an interface to the ksched kernel module
 */
#pragma once

#include <sched.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <x86gprintrin.h>

#include <base/stddef.h>
#include <base/atomic.h>
#include <base/limits.h>

#include "scheds/state.h"


#define __user

extern int ksched_fd, ksched_count;
extern struct ksched_shm_cpu *ksched_shm;
extern cpu_set_t ksched_set;
extern unsigned int ksched_gens[NCPU];
extern struct core_conn_map * cs_map;

#define MSR_P6_EVNTSEL0 0x00000186
#define MSR_P6_PERFCTR0	0xc1
#define MSR_CORE_PERF_FIXED_CTR_CTRL 0x0000038d
#define MSR_CORE_PERF_GLOBAL_CTRL    0x0000038f
#define CORE_PERF_GLOBAL_CTRL_ENABLE_PMC_0 (0x1)
#define CORE_PERF_GLOBAL_CTRL_ENABLE_PMC_1 (0x2)
DECLARE_BITMAP(input_allowed_cores, NCPU);
extern int cpu_msr_fd[NCPU];
extern uint64_t cpu_last_sel[NCPU];
extern uint64_t cpu_val[NCPU];
extern uint64_t cpu_tsc[NCPU];
extern int yield_op[NCPU];
extern int cede_op[NCPU];
extern int yield_fd[NCPU];
extern int cede_fd[NCPU];
DECLARE_BITMAP(cpu_tosend, NCPU);

#ifndef __NR_uintr_register_handler
#define __NR_uintr_register_handler	450
#define __NR_uintr_unregister_handler	451
#define __NR_uintr_create_fd		452
#define __NR_uintr_register_sender	453
#define __NR_uintr_unregister_sender	454
#define __NR_uintr_wait			455
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(fd, flags)	syscall(__NR_uintr_unregister_sender, fd, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)


/**
 * ksched_run - runs a kthread on a specific core
 * @core: the core to run a kthread on
 * @tid: the kthread's TID (or zero to idle the core)
 */
static inline void ksched_run(unsigned int core, pid_t tid)
{
	unsigned int gen = ++ksched_gens[core];

	cs_map->map[core].next_tid = tid;
	store_release(&(cs_map->map[core].gen), gen);
	bitmap_set(cpu_tosend, core);
}

/**
 * ksched_poll_run_done - determines if the last ksched_run() call finished
 * @core: the core on which kthread_run() was called
 *
 * Returns true if finished.
 */
static inline bool ksched_poll_run_done(unsigned int core)
{
	//log_info_ratelimited("last_gen: %d, gen:%d", load_acquire(&(cs_map->map[core].last_gen)), ksched_gens[core]);
	return load_acquire(&(cs_map->map[core].last_gen)) == ksched_gens[core];
}

/**
 * ksched_poll_idle - determines if a core is currently idle
 * @core: the core to check if it is idle
 *
 * Returns true if idle.
 */
static inline bool ksched_poll_idle(unsigned int core)
{
	return atomic_read(&(cs_map->map[core].idling));
}

/**
 * ksched_set_to_steal - 
 * @core: set the core to steal
 *
 * Returns true if idle.
 */
static inline bool ksched_set_to_steal(unsigned int core, bool to_steal)
{
	return cs_map->map[core].to_steal = to_steal;
}

static inline void ksched_idle_hint(unsigned int core, unsigned int hint)
{
	return;
}

enum {
	KSCHED_INTR_CEDE = 0,
	KSCHED_INTR_YIELD,
};

/**
 * ksched_enqueue_intr - enqueues an interrupt request on a core
 * @core: the core to interrupt
 * @type: the type of interrupt to enqueue
 *
 * The interrupt will not be sent until ksched_send_intrs(). This is done to
 * create an opportunity for batching interrupts. If ksched_run() is called on
 * the same core after ksched_enqueue_intr(), it may prevent interrupts
 * still pending for the last kthread from being delivered.
 */
static inline void ksched_enqueue_intr(unsigned int core, int type)
{
	int op = 0;

	switch (type) {
	case KSCHED_INTR_CEDE:
		op = cede_op[core];
		break;

	case KSCHED_INTR_YIELD:
		log_err("ksched_enqueue_intr -> KSCHED_INTR_YIELD unsupported");
		abort();
		op = yield_op[core];
		break;

	default:
		WARN();
		return;
	}
	BUG_ON(op<0);
	cs_map->map[core].op = op;
	//log_info("cs_map->map[%d].op to %d", core, op);
	ksched_count++;
	bitmap_set(cpu_tosend, core);
}

/**
 * ksched_enqueue_pmc - enqueues a performance counter request on a core
 * @core: the core to measure
 * @sel: the architecture-specific counter selector
 */
static inline void ksched_enqueue_pmc(unsigned int core, uint64_t sel)
{
	ACCESS_ONCE(cs_map->map[core].pmc_done) = 0;
	// ksched_shm[core].pmcsel = sel;
	// store_release(&ksched_shm[core].pmc, 1);
	// CPU_SET(core, &ksched_set);
	ksched_enqueue_intr(core, KSCHED_INTR_CEDE);
	ksched_count++;
}

/**
 * ksched_poll_pmc - polls for a performance counter result
 * @core: the core to poll
 * @val: a pointer to store the result
 * @tsc: a pointer to store the timestamp of the result
 *
 * Returns true if succesful, otherwise counter is still being measured.
 */
static inline bool ksched_poll_pmc(unsigned int core, uint64_t *val, uint64_t *tsc)
{
	// if (load_acquire(&ksched_shm[core].pmc) != 0)
	// 	return false;
	if (ACCESS_ONCE(cs_map->map[core].pmc_done) != 1)
	 	return false;
	*val = cs_map->map[core].pmc_val;
	*tsc = cs_map->map[core].pmc_tsc;
	return true;
}

/**
 * ksched_init_pmc
 */
static inline void ksched_init_pmc(int core)
{
	return;
}

/**
 * ksched_send_intrs - sends any pending interrupts
 */
static inline void ksched_send_intrs(void)
{
	int core;
	bitmap_for_each_set(cpu_tosend, NCPU, core) {
		if(cs_map->map[core].op == 0) continue;
#ifdef VESSEL_UIPI
		_senduipi(cs_map->map[core].op);
#else
		log_warn_first_n(1, "Vessel has been compiled without UIPI support, interrupt is disabled.");
#endif //VESSEL_UIPI
		bitmap_atomic_clear(cpu_tosend, core);
	}
}

static inline void set_ops(int core) {
#ifdef VESSEL_UIPI
	if (yield_op[core]!=-1) return;
	yield_op[core] = uintr_register_sender(yield_fd[core], 0);
	cede_op[core]  = uintr_register_sender(cede_fd[core], 0);
	if (yield_op[core]<0 || cede_op[core]<0) {
		log_err("Failed to ref uipi fd. for %s, for core: %d", strerror(errno), core);
		abort();
	}
#endif
	return;
}