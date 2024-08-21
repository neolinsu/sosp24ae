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
#define PTHREAD
#include "defs.h"

char *app_id;
#define vessel_write_log(fmt, ...) \
	printf("{ \"app_id\": \"%s\", \"val\": " fmt "}RES_END\n", app_id,  ##__VA_ARGS__)

int main(int argc, char *argv[]) {
    cpu_set_t my_cpuset;
    CPU_ZERO(&my_cpuset);
	CPU_SET(0, &my_cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(my_cpuset), &my_cpuset);
    volatile uint64_t last_done=0;
    volatile uint64_t *g_start;
    volatile uint64_t *g_id;

    struct global_meta *args = mem_map_shm(112233, NULL, sizeof(*args), PGSIZE_4KB, false);
    printf("args: %p\n", args);
    struct performance_log_entry *g_performance_log = mem_map_shm(445566, NULL, sizeof(*g_performance_log)*256, PGSIZE_4KB, false);
    g_start = &(args->g_start);
	g_id = &(args->g_id);
    app_id = args->app_id;

	printf("g_id: %ld\n", *g_id);
    while(1) {
        sleep(5);
        uint64_t done = 0;
        uint64_t cur = rdtsc();
		int num = ACCESS_ONCE(*g_id);
        for (int t=0; t<num; t++) {
            done += ACCESS_ONCE(g_performance_log[t].done);
        }
        done -= last_done;
        uint64_t lasting = cur - *g_start;
        *g_start = rdtsc();
        double time = (double) lasting;
        time /= ((double)2100*1000*1000);
		struct timespec curtime;
 		clock_gettime(CLOCK_REALTIME, &curtime);
        vessel_write_log("{\"bandwidth\": %lf, \"time\": %ld}", (done) / (time), curtime.tv_sec);
        //printf("{\"bandwidth\": %lf } RES_END\n", ((double) batch_bytes * done) / (time * 1024*1024));
        last_done += done;
    }
}