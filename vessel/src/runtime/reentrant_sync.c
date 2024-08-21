/*
 * sync.c - support for synchronization
 */

#include <base/lock.h>
#include <base/log.h>
#include <runtime/thread.h>
#include <runtime/reentrant_sync.h>

#include "defs.h"


/*
 * Mutex support
 */

#define WAITER_FLAG (1 << 31)

static inline void __reentrant_mutex_final_init(reentrant_mutex_t *m)
{
	atomic_write(&m->held, 0);
	spin_lock_init(&m->waiter_lock);
	list_head_init(&m->waiters);
	atomic_write(&m->ref, 0);
	atomic64_write(&m->holder, -1);
	barrier();
	atomic_write(&m->init_state, 2);
	barrier();
}
bool __reentrant_mutex_try_lock(reentrant_mutex_t *m){
	if(unlikely(atomic_cmpxchg(&m->init_state, 0, 1) == true)) {
		__reentrant_mutex_final_init(m);
	}
	else {
		while(atomic_read(&m->init_state) == 1) {
			cpu_relax();
		}
	}
	spin_lock_np(&m->waiter_lock);
	if(unlikely(atomic64_read(&m->holder)==(uint64_t)thread_self())) {
		atomic_fetch_and_add(&m->ref, 1);
		spin_unlock_np(&m->waiter_lock);
		return 0;
	}
	if (atomic_fetch_and_or(&m->held, WAITER_FLAG) == 0) {
		atomic_write(&m->ref, 1);
		atomic_write(&m->held, 1);
		atomic64_write(&m->holder, (uint64_t)thread_self());
		spin_unlock_np(&m->waiter_lock);
		return 0;
	}
	spin_unlock_np(&m->waiter_lock);
	return 1;

}

void __reentrant_mutex_lock(reentrant_mutex_t *m)
{

	thread_t *myth;
	// log_info("thread %p try to mutex %p lock",thread_self(),m);
	if(unlikely(atomic_cmpxchg(&m->init_state, 0, 1) == true)) {
		__reentrant_mutex_final_init(m);
	}
	else {
		while(atomic_read(&m->init_state) == 1) {
			cpu_relax();
		}
	}
	spin_lock_np(&m->waiter_lock);
	if(unlikely(atomic64_read(&m->holder)==(uint64_t)thread_self())) {
		atomic_fetch_and_add(&m->ref, 1);
		spin_unlock_np(&m->waiter_lock);
		// log_info("thread %p try to mutex %p lock, entrance success",thread_self(),m);
		return;
	}
	/* did we race with mutex_unlock? */
	if (atomic_fetch_and_or(&m->held, WAITER_FLAG) == 0) {
		atomic_write(&m->held, 1);
		atomic_write(&m->ref, 1);
		atomic64_write(&m->holder, (uint64_t)thread_self());
		spin_unlock_np(&m->waiter_lock);
		// log_info("thread %p try to mutex %p lock, normal success",thread_self(),m);
		return;
	}

	myth = thread_self();
	// log_info("list %p,myth: %p, myth->link: %p\n",&m->waiters, myth, &myth->link);
	// log_info("head %p, head->next: %p, head->prev %p\n", &m->waiters.n, m->waiters.n.prev, m->waiters.n.next);
	// log_info("p %p, p->next: %p, p->prev %p\n", p, p->prev, p->next);
	list_add_tail(&m->waiters, &myth->link);
	// log_info("thread %p lock %p failed, the owner is %p, add myself to the waiters list",thread_self(), m, (void*)atomic64_read(&m->holder));
	thread_park_and_unlock_np(&m->waiter_lock);
}


void __reentrant_mutex_unlock(reentrant_mutex_t *m)
{
	thread_t *waketh;
	spin_lock_np(&m->waiter_lock);
	// log_info("mutex %p unlock", m);
	waketh = list_pop(&m->waiters, thread_t, link);
	if(atomic_fetch_and_sub(&m->ref,1)!=1){
		return ;
	}
	// log_info("wake %p up",waketh);
	if (!waketh) {
		atomic_write(&m->held, 0);
		atomic64_write(&m->holder, 0);
		spin_unlock_np(&m->waiter_lock);
		// log_info("thread %p try to mutex %p unlock, wake %p",thread_self(),m,waketh);
		return;
	}
	atomic_write(&m->ref, 1);
	atomic64_write(&m->holder, (uint64_t)waketh);
	spin_unlock_np(&m->waiter_lock);
	thread_ready(waketh);
	// log_info("thread %p try to mutex %p unlock, wake %p",thread_self(),m,waketh);

}

/**
 * mutex_init - initializes a mutex
 * @m: the mutex to initialize
 */
void reentrant_mutex_init(reentrant_mutex_t *m)
{
	atomic_write(&m->held, 0);
	spin_lock_init(&m->waiter_lock);
	list_head_init(&m->waiters);
	atomic64_write(&m->holder, 0);
	atomic_write(&m->ref, 0);
	atomic_write(&m->init_state, 0);

	// log_info("mutex_init %p",m);
	// log_info("mutex init %p",&m->waiters);
	// log_info("mutex init %p",&m->waiters.n);
}

/*
 * Read-write mutex support
 */

/**
 * rwmutex_init - initializes a rwmutex
 * @m: the rwmutex to initialize
 */
void reentrant_rwmutex_init(rwmutex_t *m)
{
	spin_lock_init(&m->waiter_lock);
	list_head_init(&m->read_waiters);
	list_head_init(&m->write_waiters);
	m->count = 0;
	m->read_waiter_count = 0;
}

/**
 * rwmutex_rdlock - acquires a read lock on a rwmutex
 * @m: the rwmutex to acquire
 */
void reentrant_rwmutex_rdlock(rwmutex_t *m)
{
	thread_t *myth;

	spin_lock_np(&m->waiter_lock);
	myth = thread_self();
	if (m->count >= 0) {
		m->count++;
		spin_unlock_np(&m->waiter_lock);
		return;
	}
	m->read_waiter_count++;
	list_add_tail(&m->read_waiters, &myth->link);
	thread_park_and_unlock_np(&m->waiter_lock);
}

/**
 * rwmutex_try_rdlock - attempts to acquire a read lock on a rwmutex
 * @m: the rwmutex to acquire
 *
 * Returns true if the acquire was successful.
 */
bool reentrant_rwmutex_try_rdlock(rwmutex_t *m)
{
	spin_lock_np(&m->waiter_lock);
	if (m->count >= 0) {
		m->count++;
		spin_unlock_np(&m->waiter_lock);
		return true;
	}
	spin_unlock_np(&m->waiter_lock);
	return false;
}

/**
 * rwmutex_wrlock - acquires a write lock on a rwmutex
 * @m: the rwmutex to acquire
 */
void reentrant_rwmutex_wrlock(rwmutex_t *m)
{
	thread_t *myth;

	spin_lock_np(&m->waiter_lock);
	myth = thread_self();
	if (m->count == 0) {
		m->count = -1;
		spin_unlock_np(&m->waiter_lock);
		return;
	}
	list_add_tail(&m->write_waiters, &myth->link);
	thread_park_and_unlock_np(&m->waiter_lock);
}

/**
 * rwmutex_try_wrlock - attempts to acquire a write lock on a rwmutex
 * @m: the rwmutex to acquire
 *
 * Returns true if the acquire was successful.
 */
bool reentrant_rwmutex_try_wrlock(rwmutex_t *m)
{
	spin_lock_np(&m->waiter_lock);
	if (m->count == 0) {
		m->count = -1;
		spin_unlock_np(&m->waiter_lock);
		return true;
	}
	spin_unlock_np(&m->waiter_lock);
	return false;
}

/**
 * rwmutex_unlock - releases a rwmutex
 * @m: the rwmutex to release
 */
void reentrant_rwmutex_unlock(rwmutex_t *m)
{
	thread_t *th;
	struct list_head tmp;
	list_head_init(&tmp);

	spin_lock_np(&m->waiter_lock);
	assert(m->count != 0);
	if (m->count < 0)
		m->count = 0;
	else
		m->count--;

	if (m->count == 0 && m->read_waiter_count > 0) {
		m->count = m->read_waiter_count;
		m->read_waiter_count = 0;
		list_append_list(&tmp, &m->read_waiters);
		spin_unlock_np(&m->waiter_lock);
		while (true) {
			th = list_pop(&tmp, thread_t, link);
			if (!th)
				break;
			thread_ready(th);
		}
		return;
	}

	if (m->count == 0) {
		th = list_pop(&m->write_waiters, thread_t, link);
		if (!th) {
			spin_unlock_np(&m->waiter_lock);
			return;
		}
		m->count = -1;
		spin_unlock_np(&m->waiter_lock);
		thread_ready(th);
		return;
	}

	spin_unlock_np(&m->waiter_lock);

}

/*
 * Condition variable support
 */

/**
 * condvar_wait - waits for a condition variable to be signalled
 * @cv: the condition variable to wait for
 * @m: the currently held mutex that projects the condition
 */
void reentrant_condvar_wait(condvar_t *cv, reentrant_mutex_t *m)
{
	thread_t *myth;

	assert_mutex_held(m);
	spin_lock_np(&cv->waiter_lock);
	myth = thread_self();
	reentrant_mutex_unlock(m);
	list_add_tail(&cv->waiters, &myth->link);
	thread_park_and_unlock_np(&cv->waiter_lock);

	reentrant_mutex_lock(m);
}

/**
 * condvar_signal - signals a thread waiting on a condition variable
 * @cv: the condition variable to signal
 */
void reentrant_condvar_signal(condvar_t *cv)
{
	thread_t *waketh;

	spin_lock_np(&cv->waiter_lock);
	waketh = list_pop(&cv->waiters, thread_t, link);
	spin_unlock_np(&cv->waiter_lock);
	if (waketh)
		thread_ready(waketh);
}

/**
 * condvar_broadcast - signals all waiting threads on a condition variable
 * @cv: the condition variable to signal
 */
void reentrant_condvar_broadcast(condvar_t *cv)
{
	thread_t *waketh;
	struct list_head tmp;

	list_head_init(&tmp);

	spin_lock_np(&cv->waiter_lock);
	list_append_list(&tmp, &cv->waiters);
	spin_unlock_np(&cv->waiter_lock);

	while (true) {
		waketh = list_pop(&tmp, thread_t, link);
		if (!waketh)
			break;
		thread_ready(waketh);
	}
}

/**
 * condvar_init - initializes a condition variable
 * @cv: the condition variable to initialize
 */
void reentrant_condvar_init(condvar_t *cv)
{
	spin_lock_init(&cv->waiter_lock);
	list_head_init(&cv->waiters);
}


/*
 * Wait group support
 */

/**
 * waitgroup_add - adds or removes waiters from a wait group
 * @wg: the wait group to update
 * @cnt: the count to add to the waitgroup (can be negative)
 *
 * If the wait groups internal count reaches zero, the waiting thread (if it
 * exists) will be signalled. The wait group must be incremented at least once
 * before calling waitgroup_wait().
 */
void reentrant_waitgroup_add(waitgroup_t *wg, int cnt)
{
	thread_t *waketh;
	struct list_head tmp;

	list_head_init(&tmp);

	spin_lock_np(&wg->lock);
	wg->cnt += cnt;
	BUG_ON(wg->cnt < 0);
	if (wg->cnt == 0)
		list_append_list(&tmp, &wg->waiters);
	spin_unlock_np(&wg->lock);

	while (true) {
		waketh = list_pop(&tmp, thread_t, link);
		if (!waketh)
			break;
		thread_ready(waketh);
	}
}

/**
 * waitgroup_wait - waits for the wait group count to become zero
 * @wg: the wait group to wait on
 */
void reentrant_waitgroup_wait(waitgroup_t *wg)
{
	thread_t *myth;

	spin_lock_np(&wg->lock);
	myth = thread_self();
	if (wg->cnt == 0) {
		spin_unlock_np(&wg->lock);
		return;
	}
	list_add_tail(&wg->waiters, &myth->link);
	thread_park_and_unlock_np(&wg->lock);
}

/**
 * waitgroup_init - initializes a wait group
 * @wg: the wait group to initialize
 */
void reentrant_waitgroup_init(waitgroup_t *wg)
{
	spin_lock_init(&wg->lock);
	list_head_init(&wg->waiters);
	wg->cnt = 0;
}


/*
 * Barrier support
 */

/**
 * barrier_init - initializes a barrier
 * @b: the wait group to initialize
 * @count: number of threads that must wait before releasing
 */
void reentrant_barrier_init(barrier_t *b, int count)
{
	spin_lock_init(&b->lock);
	list_head_init(&b->waiters);
	b->count = count;
	b->waiting = 0;
}

/**
 * barrier_wait - waits on a barrier
 * @b: the barrier to wait on
 *
 * Returns true if the calling thread releases the barrier
 */
bool reentrant_barrier_wait(barrier_t *b)
{
	thread_t *th;
	struct list_head tmp;

	list_head_init(&tmp);

	spin_lock_np(&b->lock);

	if (++b->waiting >= b->count) {
		list_append_list(&tmp, &b->waiters);
		b->waiting = 0;
		spin_unlock_np(&b->lock);
		while (true) {
			th = list_pop(&tmp, thread_t, link);
			if (!th)
				break;
			thread_ready(th);
		}
		return true;
	}

	th = thread_self();
	list_add_tail(&b->waiters, &th->link);
	thread_park_and_unlock_np(&b->lock);
	return false;
}
