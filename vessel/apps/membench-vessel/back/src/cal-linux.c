#define _GNU_SOURCE
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <emmintrin.h>

//#include <base/mem.h>
//#include <base/lock.h>
//#include <base/cpu.h>
//#include <base/log.h>
//#include <base/assert.h>
//#include <runtime/runtime.h>
//#include <runtime/thread.h>
//#include <runtime/sync.h>
//#include <runtime/thread.h>
#define PTHREAD
#include "defs.h"
#include "cal.h"
#ifndef ACCESS_ONCE
#define	ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif
#define MAX_TO_ESCAPE 1llu<<32



const char *argp_program_version = "membench (0.1)";
const char *argp_program_bug_address = "-";
static char doc[] = "Record the addresses of the targeted event base on perf_event_open. X86 only for now.";
static struct argp_option options[] = {
    { "epoch", 'e', "#EPOCH", 0, "(8192)."},
    { "batch_size", 'b', "#BATCHSIZE(2^X)", 0, "(65536*cacheline)."},
    { "threads", 't', "#THREAD", 0, "(Config - 2)."},
    { "nop_per_batch", 'n', "#NOP_PER_BATCH", 0, "(5*65536*cacheline)."},
    { "clean_shared_mem", 'c', "#STORE TERUE", 0, "False."},
    // ...
    { 0 }
};


static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
    case 'e':
        sscanf(arg, "%lu", &args->epoch);
    break;
    case 'b':
        sscanf(arg, "%lu", &args->batch_size);
        if (args->batch_size %2) {
            return EINVAL;
        }
    break;
    case 't':
        sscanf(arg, "%lu", &args->threads);
    break;
    case 'n':
        sscanf(arg, "%lu", &args->nop_per_batch);
    break;
    case 'c':
        args->clean_shared_mem = 1;
    break;
    // ...
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

static inline
uint32_t dk_random_next(uint64_t* state) {
    (*state) = (*state) * 6364136223846793005ull + 1442695040888963407ull;
    return ((*state) >> 33);
}

static inline
uint64_t get_idx(uint32_t r, uint64_t mask) {
    return r & (~63) & (mask);
}

struct performance_log_entry *g_performance_log;


struct tls {
    int id;
};
uint64_t *g_id;
volatile uint64_t *last_done;
volatile uint64_t *g_start;

void *bench(void * a) {
    struct tls *tls = malloc(sizeof(*tls));
    tls->id = __sync_fetch_and_add(g_id, 1);
    printf("tls->id: %d\n", tls->id);
    struct arguments *args = a;
    uint64_t epoch = args->epoch;
    cpu_set_t cpuset;


	CPU_ZERO(&cpuset);
    int cpu[32] = {7,103,8,104,9,105,10,106, 11,107,12,108,13,109};
	CPU_SET(cpu[tls->id], &cpuset);

	int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if(ret==-1) {
        printf("Error for %s", strerror(errno));
        abort();
    }
    _mm_mfence();
    int c;
    for (uint64_t escape=0; escape<MAX_TO_ESCAPE; ++escape) {
        for (uint64_t epoch_i=0; epoch_i<epoch; ++epoch_i) {
            c = batch();
            if (c!=0)
                abort();
            g_performance_log[tls->id].done = g_performance_log[tls->id].done+1;
            sched_yield();
        }
    }
    return NULL;
}

struct real_main_args {
    int argc;
    void* argv;
};

void real_main(void* a) {
    struct real_main_args *cmd_args = a;
    struct arguments *args;
    struct global_meta *g_meta = mem_map_shm(112233, NULL, sizeof(*g_meta), 4096, false);
    args = malloc(sizeof(*args));
    last_done = &(g_meta->last_done);
    g_start = &(g_meta->g_start);
    g_id = &(g_meta->g_id);


    args->epoch = 8192*10000;
    args->batch_size = 8192; //1024 Pages
    args->threads = 0;
    args->nop_per_batch = 65536*400;
    args->clean_shared_mem = 0;
    // ...
    argp_parse(&argp, cmd_args->argc, cmd_args->argv, 0, 0, args);
    printf("args->epoch: %lu\n", args->epoch);
    printf("args->batch_size: %lu\n", args->batch_size);
    printf("args->threads: %lu\n", args->threads);
    printf("args->nop_per_batch: %lu\n", args->nop_per_batch);
    printf("args->clean_shared_mem: %lu\n", args->clean_shared_mem);

    g_performance_log = mem_map_shm(445566, NULL, sizeof(struct performance_log_entry) * 256, 4096, false);
    if (args->clean_shared_mem) {
        memset(g_performance_log, 0, sizeof(struct performance_log_entry)*128);
        ACCESS_ONCE(*g_id) = 0;
        *last_done=0;
        *g_start = rdtsc();
        strcpy(g_meta->app_id, "test1");
        g_meta->app_id[strlen("test1")] = '\0';
        g_meta->batch_size = args->batch_size;
    }
    pthread_t threads[args->threads];
    for (int c=0; c<args->threads; c++) {
        pthread_create(&(threads[c]), NULL, bench, args);
    }
    pthread_join(threads[0], NULL);
    while(1);
}

int main(int argc, char *argv[])
{
    struct real_main_args *cmd_args = malloc(sizeof(*cmd_args));
    if (argc<1) {
        printf("Need the path to .config\n");
        return EINVAL;
    }
    cmd_args->argc = argc;
    cmd_args->argv = &argv[0];
    real_main(cmd_args);
    //runtime_init(argv[1], real_main, cmd_args);
    return 0;
}
