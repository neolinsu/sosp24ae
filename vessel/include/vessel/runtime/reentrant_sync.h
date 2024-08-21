/*
 * sync.h - support for synchronization
 */

#pragma once

#include <base/stddef.h>
#include <base/list.h>
#include <base/lock.h>
#include <runtime/thread.h>
#include <runtime/preempt.h>
#include <stdio.h>

/*
 * Mutex support
 */

struct reentrant_mutex {
	atomic_t		held;
	spinlock_t		waiter_lock;
	struct list_head	waiters;
	atomic_t init_state ;
	atomic_t ref;
	atomic64_t holder;

};

typedef struct reentrant_mutex reentrant_mutex_t;

extern void __reentrant_mutex_lock(reentrant_mutex_t *m);
extern bool __reentrant_mutex_try_lock(reentrant_mutex_t *m);
extern void __reentrant_mutex_unlock(reentrant_mutex_t *m);
extern void reentrant_mutex_init(reentrant_mutex_t *m);

/**
 * mutex_try_lock - attempts to acquire a mutex
 * @m: the mutex to acquire
 *
 * Returns true if the acquire was successful.
 */
static inline bool reentrant_mutex_try_lock(reentrant_mutex_t *m)
{
	return __reentrant_mutex_try_lock(m);
	// return atomic_cmpxchg(&m->held, 0, 1);
}

/**
 * mutex_lock - acquires a mutex
 * @m: the mutex to acquire
 */
static inline void reentrant_mutex_lock(reentrant_mutex_t *m)
{
	__reentrant_mutex_lock(m);
}

/**
 * mutex_unlock - releases a mutex
 * @m: the mutex to release
 */
static inline void reentrant_mutex_unlock(reentrant_mutex_t *m)
{
	__reentrant_mutex_unlock(m);
}
/**
 * mutex_held - is the mutex currently held?
 * @m: the mutex to check
 */
static inline bool reentrant_mutex_held(reentrant_mutex_t *m)
{
	return atomic_read(&m->held);
}

/**
 * assert_mutex_held - asserts that a mutex is currently held
 * @m: the mutex that must be held
 */
static inline void assert_mutex_held(reentrant_mutex_t *m)
{
	assert(reentrant_mutex_held(m));
}


/*
 * Condition variable support
 */

struct condvar {
	spinlock_t		waiter_lock;
	struct list_head	waiters;
};

typedef struct condvar condvar_t;

extern void reentrant_condvar_wait(condvar_t *cv, reentrant_mutex_t *m);
extern void reentrant_condvar_signal(condvar_t *cv);
extern void reentrant_condvar_broadcast(condvar_t *cv);
extern void reentrant_condvar_init(condvar_t *cv);


/*
 * Wait group support
 */

struct waitgroup {
	spinlock_t		lock;
	int			cnt;
	struct list_head	waiters;
};

typedef struct waitgroup waitgroup_t;

extern void reentrant_waitgroup_add(waitgroup_t *wg, int cnt);
extern void reentrant_waitgroup_wait(waitgroup_t *wg);
extern void reentrant_waitgroup_init(waitgroup_t *wg);

/**
 * waitgroup_done - notifies the wait group that one waiting event completed
 * @wg: the wait group to complete
 */
static inline void reentrant_waitgroup_done(waitgroup_t *wg)
{
	reentrant_waitgroup_add(wg, -1);
}


/*
 * Spin lock support
 */

/**
 * spin_lock_np - takes a spin lock and disables preemption
 * @l: the spin lock
 */
static inline void spin_lock_np(spinlock_t *l)
{
	preempt_disable();
	spin_lock(l);
}

/**
 * spin_try_lock_np - takes a spin lock if its available and disables preemption
 * @l: the spin lock
 *
 * Returns true if successful, otherwise fail.
 */
static inline bool spin_try_lock_np(spinlock_t *l)
{
	preempt_disable();
	if (spin_try_lock(l))
		return true;

	preempt_enable();
	return false;
}

/**
 * spin_unlock_np - releases a spin lock and re-enables preemption
 * @l: the spin lock
 */
static inline void spin_unlock_np(spinlock_t *l)
{
	spin_unlock(l);
	preempt_enable();
}


/*
 * Barrier support
 */

struct barrier {
	spinlock_t		lock;
	int			waiting;
	int			count;
	struct list_head	waiters;
};

typedef struct barrier barrier_t;

extern void reentrant_barrier_init(barrier_t *b, int count);
extern bool reentrant_barrier_wait(barrier_t *b);


/*
 * Read-write mutex support
 */

struct rwmutex {
	spinlock_t		waiter_lock;
	int			count;
	struct list_head	read_waiters;
	struct list_head	write_waiters;
	int			read_waiter_count;
};

typedef struct rwmutex rwmutex_t;

extern void reentrant_rwmutex_init(rwmutex_t *m);
extern void reentrant_rwmutex_rdlock(rwmutex_t *m);
extern void reentrant_rwmutex_wrlock(rwmutex_t *m);
extern bool reentrant_rwmutex_try_rdlock(rwmutex_t *m);
extern bool reentrant_rwmutex_try_wrlock(rwmutex_t *m);
extern void reentrant_rwmutex_unlock(rwmutex_t *m);
