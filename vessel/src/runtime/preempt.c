/*
 * preempt.c - support for kthread preemption
 */

#include <signal.h>
#include <string.h>

#include <vessel/interface.h>

#include <base/log.h>
#include <base/cpu.h>
#include <runtime/thread.h>
#include <runtime/preempt.h>
#include <scheds/state.h>
#include "defs.h"

/* the current preemption count */
volatile __thread unsigned int preempt_cnt = PREEMPT_NOT_PENDING;
volatile __thread bool preempt_cede;

/* set a flag to indicate a preemption request is pending */
static void set_preempt_needed(void)
{
	preempt_cnt &= ~PREEMPT_NOT_PENDING;
}

/* handles preemption cede signals from the iokernel */
static void handle_cede(void)
{
	preempt_cede = true;
	set_preempt_needed();
	barrier();
	STAT(PREEMPTIONS)++;
	/* resume execution if preemption is disabled */
	if (!preempt_enabled())
		return;
	preempt_disable();
	thread_cede_hard();
}

/* handles preemption yield signals from the iokernel */
static void handle_yield(void)
{
	log_err("Unspported!");
	abort();
}

/**
 * preempt_hard - entry point for preemption
 */
void preempt_hard(void)
{
	preempt_disable();
	clear_preempt_needed();
	thread_cede_hard();
}

/**
 * preempt - entry point for preemption
 */
void preempt(void)
{
	getk();
	if (!preempt_needed()) {
		putk();
		return;
	}
	clear_preempt_needed();
	thread_cede();
}

spinlock_t uipi_fd_l;
LIST_HEAD(yield_fd_list);
LIST_HEAD(cede_fd_list);
volatile int uipi_fd_cnt = 0;
/**
 * preempt_init - global initializer for preemption support
 *
 * Returns 0 if successful. otherwise fail.
 */
int preempt_init_thread(void)
{
#ifndef VESSEL_ORI

	struct uipi_ops_inf *ops = malloc(sizeof (struct uipi_ops_inf));
	ops->to_cede = &handle_cede;
	ops->to_yield = &handle_yield;
	int uipi_vector_cede_fd;
	int uipi_vector_yield_fd;
	int ret = vessel_register_uipi_handlers(
		ops,
		&uipi_vector_cede_fd,
		&uipi_vector_yield_fd
	);
	if (ret) {
		log_err("vessel_register_uipi_handlers failed");
	}
	struct uipi_fd *yield_fd = malloc(sizeof(struct uipi_fd));
	struct uipi_fd *cede_fd = malloc(sizeof(struct uipi_fd));
	spin_lock(&uipi_fd_l);
	yield_fd->fd = uipi_vector_yield_fd;
	yield_fd->core_id = get_cur_cpuid();
	cede_fd->fd = uipi_vector_cede_fd;
	cede_fd->core_id = get_cur_cpuid();

	list_add_tail(&yield_fd_list, &yield_fd->link);
	list_add_tail(&cede_fd_list, &cede_fd->link);
	uipi_fd_cnt++;
	barrier();
	spin_unlock(&uipi_fd_l);
#endif //VESSEL_ORI
	log_info("preempt: %d", uipi_fd_cnt);
	return 0;
}

int preempt_init(void) {
	spin_lock_init(&uipi_fd_l);
	return 0;
}