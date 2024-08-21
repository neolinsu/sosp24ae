/*
 * lock.h - locking primitives
 */

#pragma once

#include<x86gprintrin.h>

#include <base/stddef.h>
#include <base/log.h>
#include <asm/ops.h>

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
	while (__sync_lock_test_and_set(&l->locked, 1)) {
#ifdef DEBUG
		uint64_t stime = rdtsc();
#endif
		while (l->locked) {
			cpu_relax();
#ifdef DEBUG
			if(rdtsc() - stime > 2200ll*1000*1000) {
				log_info("%llx Wait lock for too long.", _readfsbase_u64());
			}
#endif
		}
	}
}

/**
 * spin_try_lock- takes a spin lock, but only if it is available
 * @l: the spin lock
 *
 * Returns 1 if successful, otherwise 0
 */
static inline bool spin_try_lock(spinlock_t *l)
{
	if (!__sync_lock_test_and_set(&l->locked, 1))
		return true;
	return false;
}

/**
 * spin_unlock - releases a spin lock
 * @l: the spin lock
 */
static inline void spin_unlock(spinlock_t *l)
{
	assert_spin_lock_held(l);
	__sync_lock_release(&l->locked);
}
