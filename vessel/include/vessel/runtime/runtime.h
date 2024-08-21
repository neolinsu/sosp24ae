/*
 * runtime.h - runtime initialization and metrics
 */

#pragma once

#include <base/stddef.h>
#include <base/time.h>
#include <runtime/thread.h>


/* main initialization */
typedef int (*initializer_fn_t)(void);

extern int runtime_set_initializers(initializer_fn_t global_fn,
				    initializer_fn_t perthread_fn,
				    initializer_fn_t late_fn);
extern int runtime_init(const char *cfgpath, thread_fn_t main_fn, void *arg);


extern struct congestion_info *runtime_congestion;

extern unsigned int maxks;
extern unsigned int guaranteedks;
extern atomic_t runningks;

/**
 * runtime_queue_us - returns the us of packet queueing delay + runtime queueing
 * delay
 */
static inline uint64_t runtime_queue_us(void)
{
	return ACCESS_ONCE(runtime_congestion->delay_us);
}

/**
 * runtime_load - returns the current CPU usage (number of cores)
 */
static inline float runtime_load(void)
{
	return ACCESS_ONCE(runtime_congestion->load);
}

/**
 * runtime_active_cores - returns the number of currently active cores
 *
 */
static inline int runtime_active_cores(void)
{
	return atomic_read(&runningks);
}

/**
 * runtime_max_cores - returns the maximum number of cores
 *
 * The runtime could be given at most this number of cores by the IOKernel.
 */
static inline int runtime_max_cores(void)
{
	return maxks;
}

/**
 * runtime_guaranteed_cores - returns the guaranteed number of cores
 *
 * The runtime will get at least this number of cores by the IOKernel if it
 * requires them.
 */
static inline int runtime_guaranteed_cores(void)
{
	return guaranteedks;
}

extern char app_id[128];

#define vessel_write_log(fmt, ...) \
	printf("{ \"app_id\": \"%s\", \"val\": " fmt "}RES_END\n", app_id,  ##__VA_ARGS__)

