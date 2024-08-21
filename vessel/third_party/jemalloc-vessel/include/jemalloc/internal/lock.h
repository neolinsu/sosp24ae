#ifndef JEMALLOC_INTERNAL_LOCK_H
#define JEMALLOC_INTERNAL_LOCK_H
#include <stdbool.h>
#include <stdbool.h>
#include <stdint.h>


static inline void cpu_relax(void)
{
	asm volatile("pause");
}

static inline void cpu_serialize(void)
{
        asm volatile("xorl %%eax, %%eax\n\t"
		     "cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");
}

static inline uint64_t rdtsc(void)
{
	uint32_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((uint64_t)a) | (((uint64_t)d) << 32);
}

static inline uint64_t rdtscp(uint32_t *auxp)
{
	uint32_t a, d, c;
	asm volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
	if (auxp)
		*auxp = c;
	return ((uint64_t)a) | (((uint64_t)d) << 32);
}

static inline void _fnstenv(void* ptr)
{
	asm volatile("fnstenv 0(%0)" : : "a" (ptr));
	return;
}

static inline void _stmxcsr(void* ptr)
{
	asm volatile("stmxcsr 0(%0)" : :"a" (ptr));
	return;
}

static inline uint32_t rdpid_safe()
{
	uint32_t a, d, c;
	asm volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
	c &= 0xFFF;
	return c;
}

static inline uint64_t __mm_crc32_u64(uint64_t crc, uint64_t val)
{
	asm("crc32q %1, %0" : "+r" (crc) : "rm" (val));
	return crc;
}


typedef struct {
	volatile int locked;
} spinlock_t;


#define SPINLOCK_INITIALIZER {.locked = 0}
#define DEFINE_SPINLOCK(name) spinlock_t name = SPINLOCK_INITIALIZER
#define DECLARE_SPINLOCK(name) extern spinlock_t name

/**
 * spin_lock_init - prepares a spin lock for use
 * @l: the spin lock
 */
static inline void spin_lock_init(spinlock_t *l)
{
	l->locked = 0;
}

/**
 * spin_lock_held - determines if the lock is held
 * @l: the spin lock
 *
 * Returns true if the lock is held.
 */
static inline bool spin_lock_held(spinlock_t *l)
{
	return l->locked != 0;
}

/**
 * assert_spin_lock_held - asserts that the lock is currently held
 * @l: the spin lock
 */
static inline void assert_spin_lock_held(spinlock_t *l)
{
	assert(spin_lock_held(l));
}

/**
 * spin_lock - takes a spin lock
 * @l: the spin lock
 */
static inline void spin_lock(spinlock_t *l)
{
	// printf("jemalloc -> spin_lock\n");
	while (__sync_lock_test_and_set(&l->locked, 1)) {
		while (l->locked)
			cpu_relax();
	}
}

/**
 * spin_try_lock- takes a spin lock, but only if it is available
 * @l: the spin lock
 *
 * Returns 1 if successful, otherwise 0
 */
static inline int spin_try_lock(spinlock_t *l)
{
	// malloc_printf("cpu %d, try to lock %p\n",sched_getcpu(),l);
	if (!__sync_lock_test_and_set(&l->locked, 1)){
		// malloc_printf("cpu %d, success to lock %p\n",sched_getcpu(),l);
		return 1;
	}
	// malloc_printf("cpu %d failed to lock %p\n",sched_getcpu(),l);
	return 0;
}

/**
 * spin_unlock - releases a spin lock
 * @l: the spin lock
 */
static inline void spin_unlock(spinlock_t *l)
{
	// malloc_printf("cpu %d try to unlock %p\n",sched_getcpu(),l);
	if(unlikely(!spin_lock_held(l))){
		// malloc_printf("cpu %d wtf are you want to unlock %p\n",sched_getcpu(),l);
	}
	assert_spin_lock_held(l);
	__sync_lock_release(&l->locked);
}


#endif /* JEMALLOC_INTERNAL_LOCK_H */