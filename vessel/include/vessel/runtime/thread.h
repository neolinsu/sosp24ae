/*
 * thread.h - support for user-level threads
 */

#pragma once

#include <base/types.h>
#include <base/compiler.h>
#include <runtime/preempt.h>
#include <iokernel/control.h>
#include <vcontext/vcontext.h>

struct thread;
typedef void (*thread_fn_t)(void *arg);
typedef struct thread thread_t;


/*
 * Low-level routines, these are helpful for bindings and synchronization
 * primitives.
 */

extern void thread_park_and_unlock_np(spinlock_t *l);
extern void thread_park_and_preempt_enable(void);
extern void thread_ready(thread_t *thread);
extern void thread_ready_head(thread_t *thread);
extern thread_t *thread_create(thread_fn_t fn, void *arg);
extern thread_t *thread_create_with_buf(thread_fn_t fn, void **buf, size_t len);

extern __thread thread_t *__self;
extern __thread unsigned int kthread_idx;

static inline unsigned int get_current_affinity(void)
{
	return kthread_idx;
}

/**
 * thread_self - gets the currently running thread
 */
inline thread_t *thread_self(void)
{
	return __self;
}


extern uint64_t get_uthread_specific(void);
extern void set_uthread_specific(uint64_t val);


/*
 * High-level routines, use this API most of the time.
 */

extern void thread_yield(void);
extern void thread_yield_with_ctx(const vcontext_t *ctx);
extern int thread_spawn(thread_fn_t fn, void *arg);
extern void thread_exit(void) __noreturn;

extern void lock_directpath(void);
extern void unlock_directpath(void);
extern void wait_directpath_busy(void);
extern bool softirq_do_directpath(void);