#pragma once
#include <stdint.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <linux/mman.h>

#include <sched.h>
#include <stddef.h>
#include <numaif.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/shm.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

struct arguments {
    uint64_t epoch;
    uint64_t batch_size;
    uint64_t threads;
    uint64_t nop_per_batch;
    uint64_t clean_shared_mem;
    // ...
};

struct global_meta {
    uint64_t last_done;
    uint64_t g_start;
    uint64_t batch_size;
    uint64_t g_id;
	uint64_t nop_per_batch;
    char  app_id[128];
};

struct performance_log_entry {
    volatile uint64_t done;
    volatile uint64_t nop_tsc;
    volatile uint64_t mem_tsc;
    char pad[CACHE_LINE_SIZE - sizeof(uint64_t) * 3];
};

#ifdef PTHREAD
#define NCPU 128

static inline void cpu_relax(void)
{
	asm volatile("pause");
}

static inline uint64_t rdtsc(void)
{
#if __GNUC_PREREQ(10, 0)
#  if __has_builtin(__builtin_ia32_rdtsc)
	return __builtin_ia32_rdtsc();
#  endif
#else
	uint64_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return a | (d << 32);
#endif
}

#define	ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
typedef unsigned int mem_key_t;
static void touch_mapping(void *base, size_t len, size_t pgsize)
{
	char *pos;
	for (pos = (char *)base; pos < (char *)base + len; pos += pgsize)
		ACCESS_ONCE(*pos);
} 

enum {
	PGSHIFT_4KB = 12,
	PGSHIFT_2MB = 21,
	PGSHIFT_1GB = 30,
};

enum {
	PGSIZE_4KB = (1 << PGSHIFT_4KB), /* 4096 bytes */
	PGSIZE_2MB = (1 << PGSHIFT_2MB), /* 2097152 bytes */
	PGSIZE_1GB = (1 << PGSHIFT_1GB), /* 1073741824 bytes */
};

static void *__mem_map_shm(mem_key_t key, void *base, size_t len,
		  size_t pgsize, bool exclusive, bool rdonly)
{
	void *addr;
	int shmid, flags = rdonly ? 0 : (IPC_CREAT | 0744);

	switch (pgsize) {
	case PGSIZE_4KB:
		break;
	case PGSIZE_2MB:
		flags |= SHM_HUGETLB;
#ifdef SHM_HUGE_2MB
		flags |= SHM_HUGE_2MB;
#endif
		break;
	case PGSIZE_1GB:
#ifdef SHM_HUGE_1GB
		flags |= SHM_HUGETLB | SHM_HUGE_1GB;
#else
		return MAP_FAILED;
#endif
		break;
	default: /* fail on other sizes */
		return MAP_FAILED;
	}

	if (exclusive)
		flags |= IPC_EXCL;

	shmid = shmget(key, len, flags);
	if (shmid == -1)
		return MAP_FAILED;

	flags = rdonly ? SHM_RDONLY : 0;
	addr = shmat(shmid, base, flags);
	if (addr == MAP_FAILED)
		return MAP_FAILED;

	touch_mapping(addr, len, pgsize);
	return addr;
}

/**
 * mem_map_shm - maps a System V shared memory segment
 * @key: the unique key that identifies the shared region (e.g. use ftok())
 * @base: the base address to map the shared segment (or automatic if NULL)
 * @len: the length of the mapping
 * @pgsize: the size of each page
 * @exclusive: ensure this call creates the shared segment
 *
 * Returns a pointer to the mapping, or NULL if the mapping failed.
 */
void *mem_map_shm(mem_key_t key, void *base, size_t len, size_t pgsize,
		  bool exclusive)
{
	return __mem_map_shm(key, base, len, pgsize, exclusive, false);
}
#endif