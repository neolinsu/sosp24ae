
#include <dlfcn.h>
#include <pthread.h>

#include <runtime/reentrant_sync.h>

BUILD_ASSERT(sizeof(pthread_barrier_t) >= sizeof(barrier_t));
BUILD_ASSERT(sizeof(pthread_mutex_t) >= sizeof(reentrant_mutex_t));
BUILD_ASSERT(sizeof(pthread_spinlock_t) >= sizeof(spinlock_t));
BUILD_ASSERT(sizeof(pthread_cond_t) >= sizeof(condvar_t));
BUILD_ASSERT(sizeof(pthread_rwlock_t) >= sizeof(rwmutex_t));

#define NOTSELF_1ARG(retType, name, arg)                                       \
	if (unlikely(!__self)) {                                               \
		static retType (*fn)(typeof(arg));                             \
		if (!fn) {                                                     \
			fn = dlsym(RTLD_NEXT, name);                           \
			BUG_ON(!fn);                                           \
		}                                                              \
		return fn(arg);                                                \
	}

#define NOTSELF_2ARG(retType, name, arg1, arg2)                                \
	if (unlikely(!__self)) {                                               \
		static retType (*fn)(typeof(arg1), typeof(arg2));              \
		if (!fn) {                                                     \
			fn = dlsym(RTLD_NEXT, name);                           \
			BUG_ON(!fn);                                           \
		}                                                              \
		return fn(arg1, arg2);                                         \
	}

#define NOTSELF_3ARG(retType, name, arg1, arg2, arg3)                          \
	if (unlikely(!__self)) {                                               \
		static retType (*fn)(typeof(arg1), typeof(arg2),               \
				     typeof(arg3));                            \
		if (!fn) {                                                     \
			fn = dlsym(RTLD_NEXT, name);                           \
			BUG_ON(!fn);                                           \
		}                                                              \
		return fn(arg1, arg2, arg3);                                   \
	}

int pthread_mutex_init(pthread_mutex_t *mutex,
		       const pthread_mutexattr_t *mutexattr)
{
	static int (*fn)(pthread_mutex_t *,
		       const pthread_mutexattr_t *);
	if (unlikely(!__self)) {
		if (!fn) {
			fn = dlsym(RTLD_NEXT, "pthread_mutex_init");
		}
		return fn(mutex, mutexattr);
	}
	reentrant_mutex_init((reentrant_mutex_t *)mutex);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	static int (*fn)(pthread_mutex_t *);
	if (unlikely(!__self)) {
		if (!fn) {
			fn = dlsym(RTLD_NEXT, "pthread_mutex_lock");
		}
		return fn(mutex);
	}
	// fprintf(stderr,"%d: successfully into mutex hook at p %p\n",sched_getcpu(),mutex);
	reentrant_mutex_lock((reentrant_mutex_t *)mutex);
	// fprintf(stderr,"%d: successfully into mutex hook at p return %p\n",sched_getcpu(),mutex);
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	static int (*fn)(pthread_mutex_t *);

	if (unlikely(!__self)) {
		if (!fn) {
			fn = dlsym(RTLD_NEXT, "pthread_mutex_trylock");
		}
		return fn(mutex);
	}
	return reentrant_mutex_try_lock((reentrant_mutex_t *)mutex) ? 0 : EBUSY;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	static int (*fn)(pthread_mutex_t *);

	if (unlikely(!__self)) {
		if (!fn) {
			fn = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
		}
		return fn(mutex);
	}
	// fprintf(stderr,"%d: successfully into mutex unlock hook at p %p\n",sched_getcpu(),mutex);
	reentrant_mutex_unlock((reentrant_mutex_t *)mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	// fprintf(stderr,"%d into mutex destroy hook at p %p",sched_getcpu(),mutex);
	NOTSELF_1ARG(int, __func__, mutex);
	return 0;
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier,
			 const pthread_barrierattr_t *restrict attr,
			 unsigned count)
{
	NOTSELF_3ARG(int, __func__, barrier, attr, count);

	reentrant_barrier_init((barrier_t *)barrier, count);

	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
	NOTSELF_1ARG(int, __func__, barrier);

	if (reentrant_barrier_wait((barrier_t *)barrier))
		return PTHREAD_BARRIER_SERIAL_THREAD;

	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	NOTSELF_1ARG(int, __func__, barrier);
	return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock)
{
	NOTSELF_1ARG(int, __func__, lock);
	return 0;
}

int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
	NOTSELF_2ARG(int, __func__, lock, pshared);
	spin_lock_init((spinlock_t *)lock);
	return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock)
{
	NOTSELF_1ARG(int, __func__, lock);
	spin_lock_np((spinlock_t *)lock);
	return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *lock)
{
	NOTSELF_1ARG(int, __func__, lock);
	return spin_try_lock_np((spinlock_t *)lock) ? 0 : EBUSY;
}

int pthread_spin_unlock(pthread_spinlock_t *lock)
{
	NOTSELF_1ARG(int, __func__, lock);
	spin_unlock_np((spinlock_t *)lock);
	return 0;
}

int pthread_cond_init(pthread_cond_t *__restrict cond,
		      const pthread_condattr_t *__restrict cond_attr)
{
	NOTSELF_2ARG(int, __func__, cond, cond_attr);
	reentrant_condvar_init((condvar_t *)cond);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	NOTSELF_1ARG(int, __func__, cond);
	reentrant_condvar_signal((condvar_t *)cond);
	return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	NOTSELF_1ARG(int, __func__, cond);
	reentrant_condvar_broadcast((condvar_t *)cond);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	NOTSELF_2ARG(int, __func__, cond, mutex);
	reentrant_condvar_wait((condvar_t *)cond, (reentrant_mutex_t *)mutex);
	return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *abstime)
{
	BUG();
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	NOTSELF_1ARG(int, __func__, cond);
	return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *r, const pthread_rwlockattr_t *attr)
{
	NOTSELF_2ARG(int, __func__, r, attr);
	reentrant_rwmutex_init((rwmutex_t *)r);
	return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	reentrant_rwmutex_rdlock((rwmutex_t *)r);
	return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	return reentrant_rwmutex_try_rdlock((rwmutex_t *)r) ? 0 : EBUSY;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	return reentrant_rwmutex_try_wrlock((rwmutex_t *)r) ? 0 : EBUSY;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	reentrant_rwmutex_wrlock((rwmutex_t *)r);
	return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *r)
{
	NOTSELF_1ARG(int, __func__, r);
	reentrant_rwmutex_unlock((rwmutex_t *)r);
	return 0;
}
