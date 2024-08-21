/*
 * cpu.h - detection for CPU topology
 */

#pragma once

#include <immintrin.h>
#include <stdint.h>

#include <base/stddef.h>
#include <base/limits.h>
#include <base/bitmap.h>
#include <base/lock.h>
#include <base/stringify.h>

#include <asm/ops.h>
#include <asm/x86_ext_fixup_types.h>
#include <asm/x86_fpu.h>



struct siblings_info {
	DEFINE_BITMAP(thread_siblings_mask, NCPU);
	DEFINE_BITMAP(core_siblings_mask, NCPU);
};
typedef struct siblings_info siblings_info_t;

struct cpu_info {
	int cpu_count;
	int numa_count;
	DEFINE_BITMAP(cpu_mask, NCPU);
	DEFINE_BITMAP(numa_mask, NNUMA);
	siblings_info_t map[NCPU];
};
typedef struct cpu_info cpu_info_t;
extern cpu_info_t cpu_info_tbl;
struct cpu_state_map {
	spinlock_t lock;
	DEFINE_BITMAP(state, NCPU);
};
typedef struct cpu_state_map cpu_state_map_t;


extern int cpu_alloc(cpu_info_t *cpu_info, bitmap_ptr_t affinitive_cpus, int exclusive);

extern int cpu_init();

static inline int get_cur_cpuid() {
	return rdpid_safe();
}

static inline void loadfs(long long unsigned int fs) {
	_writefsbase_u64(fs);
	return;
}

static inline void savefs(long long unsigned int *fs) {
	*fs = _readfsbase_u64();
	return;
}

static inline void save_envs(void* fpstate, void* xmstate) {
	_fnstenv(fpstate);
	_stmxcsr(xmstate);
	return;
}

static inline uint64_t get_cur_fs() {
    return _readfsbase_u64();
};


static inline void os_xsave(struct fpstate *fpstate)
{
	uint64_t mask = ~0 & (~(1<<9));
	uint32_t lmask = mask;
	uint32_t hmask = mask >> 32;
	int err;

	XSTATE_XSAVE(&(fpstate->regs), lmask, hmask, err);
}

static inline void os_xrstor(struct fpstate *fpstate)
{
	uint64_t mask = ~0 & (~(1<<9));
	uint32_t lmask = mask;
	uint32_t hmask = mask >> 32;

	XSTATE_XRESTORE(&fpstate->regs, lmask, hmask);
}