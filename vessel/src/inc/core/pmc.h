#pragma once
#include <unistd.h>
#include <x86intrin.h>
#include <emmintrin.h>

#include <base/compiler.h>
#include <base/stddef.h>


/**
 * rdpmc_read - read a ring 3 readable performance counter
 * @ctx: Pointer to initialized &rdpmc_ctx structure.
 *
 * Read the current value of a running performance counter.
 * This should only be called from the same thread/process as opened
 * the context. For new threads please create a new context.
 */
static inline unsigned long long rdpmc_read(unsigned index)
{
	uint64_t val = _rdpmc(index);
	barrier();
	return (val + 0x7ffffffffffflu) & 0xfffffffffffflu;
}

extern int pmc_init_perthread(void);