#define _GNU_SOURCE
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
#include "defs.h"
#define	ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

typedef unsigned int mem_key_t;

static void touch_mapping(void *base, size_t len, size_t pgsize)
{
	char *pos;
	for (pos = (char *)base; pos < (char *)base + len; pos += pgsize)
		ACCESS_ONCE(*pos);
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
char *app_id;
#define vessel_write_log(fmt, ...) \
	printf("{ \"app_id\": \"%s\", \"val\": " fmt "}RES_END\n", app_id,  ##__VA_ARGS__)

int key1, key2;

int main(int argc, char *argv[]) {
    cpu_set_t my_cpuset;
    CPU_ZERO(&my_cpuset);
	CPU_SET(0, &my_cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(my_cpuset), &my_cpuset);
    volatile uint64_t *last_done;
    volatile uint64_t *g_start;
    volatile uint64_t *g_id;
	uint64_t last_nop_tsc = 0;
	uint64_t last_mem_tsc = 0;
    char *ckey1, *ckey2;
    ckey1 = getenv("MKEY1");
    ckey2 = getenv("MKEY2");
    key1 = atoi(ckey1);
    key2 = atoi(ckey2);

    struct global_meta *args = mem_map_shm(key1, NULL, sizeof(*args), PGSIZE_4KB, false);
    printf("args: %p\n", args);
    struct performance_log_entry *g_performance_log = mem_map_shm(key2, NULL, sizeof(*g_performance_log)*128, PGSIZE_4KB, false);
    last_done = &(args->last_done);
    g_start = &(args->g_start);
	g_id = &(args->g_id);

    uint64_t batch_size = ACCESS_ONCE(args->batch_size);
    uint64_t batch_bytes = batch_size * CACHE_LINE_SIZE;
	int nop_count[20]={16384,32768,65536};
	for (int i=0;i<20;i++){
			nop_count[i]=2048*5;//16384*(20-i);
	}

	printf("g_id: %ld\n", *g_id);
	int times = 0;
	*last_done = 0;
    while(1) {
		ACCESS_ONCE(args->nop_per_batch)=nop_count[times%20];
        sleep(5);
        uint64_t done = 0;
        uint64_t nop_tsc = 0;
        uint64_t mem_tsc = 0;
        uint64_t cur = rdtsc();
		int num = ACCESS_ONCE(*g_id);
        for (int t=0; t<num; t++) {
            done += ACCESS_ONCE(g_performance_log[t].done);
			nop_tsc += ACCESS_ONCE(g_performance_log[t].nop_tsc);
			mem_tsc += ACCESS_ONCE(g_performance_log[t].mem_tsc);
		}
        done -= *last_done;
		nop_tsc -= last_nop_tsc;
		mem_tsc -= last_mem_tsc;
        uint64_t lasting = cur - *g_start;
        *g_start = rdtsc();
        double time = (double) lasting;
        time /= ((double)2100*1000*1000);
		struct timespec curtime;
 		clock_gettime(CLOCK_REALTIME, &curtime);
        printf("{\"bandwidth\": %lf, \"time\": %ld, \"nop_per_batch\": %lu, \"done\":%lu \"batch_bytes\": %lu, \"nop_us\": %lf, \"mem_us\": %lf}\n",
				((double) batch_bytes * done) / (time * 1024*1024),
				curtime.tv_sec,
				args->nop_per_batch,
				done,
				batch_bytes,
				(double)nop_tsc/(2100.0*done),
				(double)mem_tsc/(2100.0*done));
		times ++;
        *last_done += done;
		last_nop_tsc += nop_tsc;
		last_mem_tsc += mem_tsc;
    }
}