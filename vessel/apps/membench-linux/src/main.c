#define _GNU_SOURCE
#define PTHREAD
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <emmintrin.h>

#include <base/atomic.h>

#include "defs.h"

#define MAX_TO_ESCAPE 1llu<<32

struct global_meta *g_meta;

struct signal {
    atomic_t cnt;
    atomic_t out;
};
int key1, key2;

static inline void signal_init(struct signal *sig, uint32_t len) {
    atomic_write(&sig->cnt, len);
    atomic_write(&sig->out, len);
}

static inline bool signal_wait(struct signal *sig) {
    while(true) {
        cpu_relax();
        bool ret;
        int cur = atomic_read(&sig->cnt);
        if (cur > 1) {
            ret = atomic_cmpxchg(&sig->cnt, cur, cur - 1);
            if (ret) {
                while(atomic_read(&sig->cnt)>0)
                    cpu_relax();
                atomic_dec(&sig->out);
                return false;
            }
        } else if (cur == 1) {
            ret = atomic_cmpxchg(&sig->cnt, cur, 0);
            if (ret) {
                atomic_dec(&sig->out);
                while(atomic_read(&sig->out)!=0)
                    cpu_relax();
                return true;
            }
        } else {
            continue;
        }
    }
}

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
        sscanf(arg, "%ld", &args->clean_shared_mem);
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

static inline
void nt_store(char* buf, int c) {
    __m128i a = _mm_set_epi32(c, c, c, c);
    _mm_store_si128((__m128i *)buf     , a);
    _mm_store_si128((__m128i *)(buf+16), a);
    _mm_store_si128((__m128i *)(buf+32), a);
    _mm_store_si128((__m128i *)(buf+48), a);
    // _mm_stream_si128((__m128i *)buf     , a);
    // _mm_stream_si128((__m128i *)(buf+16), a);
    // _mm_stream_si128((__m128i *)(buf+32), a);
    // _mm_stream_si128((__m128i *)(buf+48), a);
}

struct performance_log_entry *g_performance_log;

struct tls {
    int id;
};
uint64_t *g_id;
volatile uint64_t *last_done;
volatile uint64_t *g_start;

void* bench(void * a) {
    struct tls *tls = malloc(sizeof(*tls));
    tls->id = __sync_fetch_and_add(g_id, 1);
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);    // 初始化CPU集合，将 cpuset 置为空
    CPU_SET(5+tls->id, &cpuset);  // 将本进程绑定到 CPU2 上

    // 设置进程的 CPU 亲和性
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
        printf("Set CPU affinity failed, error: %s\n", strerror(errno));
        return NULL;
    }
    struct arguments *args = a;
    uint64_t epoch = args->epoch,
        batch_size = args->batch_size,
        nop_per_batch = args->nop_per_batch;
    size_t area_size=512ll*1024*1024; // 512 MB
    char* buf = aligned_alloc(CACHE_LINE_SIZE, area_size);
    memset(buf, -1, area_size);
    uint64_t rand_state = rdtsc();
    int c = (int) rand_state;
    uint64_t time_tsc = 0;
    // uint64_t range_mask = batch_size - 1;
    for (uint64_t escape=0; escape<MAX_TO_ESCAPE; ++escape) {
        for (uint64_t epoch_i=0; epoch_i<epoch; ++epoch_i) {
            batch_size = ACCESS_ONCE(g_meta->batch_size);
            time_tsc = rdtsc();
            for (uint64_t batch_i=0; batch_i<(batch_size>>2); ++batch_i) {
                int nxt=dk_random_next(&rand_state)&(area_size/CACHE_LINE_SIZE-1);
                char *ptr = buf +  (nxt <<6);
                nt_store(ptr, c);
                nxt++;
                ptr = buf + (nxt <<6);
                nt_store(ptr, c);
                nxt++;
                ptr = buf +  (nxt <<6);
                nt_store(ptr, c);
                nxt++;
                ptr = buf +  (nxt <<6);
                nt_store(ptr, c);
                nxt++;
            }
            uint64_t curtsc = rdtsc();
            g_performance_log[tls->id].mem_tsc += curtsc - time_tsc;
            time_tsc = curtsc;
            _mm_mfence();
            nop_per_batch=ACCESS_ONCE(g_meta->nop_per_batch);
            for(uint64_t nop_i=0; nop_i<nop_per_batch; ++nop_i) {
                __asm volatile ("nop");
            }
            g_performance_log[tls->id].nop_tsc += rdtsc() - time_tsc;
            ACCESS_ONCE(g_performance_log[tls->id].done) = g_performance_log[tls->id].done+1;
        }
        sched_yield();
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
    char *ckey1, *ckey2;
    ckey1 = getenv("MKEY1");
    ckey2 = getenv("MKEY2");
    key1 = atoi(ckey1);
    key2 = atoi(ckey2);
    g_meta = mem_map_shm(key1, NULL, sizeof(*g_meta), PGSIZE_4KB, false);
    printf("g_meta: %p\n", g_meta);
    args = malloc(sizeof(*args));
    last_done = &(g_meta->last_done);
    g_start = &(g_meta->g_start);
    g_id = &(g_meta->g_id);

    args->epoch = 8192*10000;
    args->batch_size = 8192*256; //1024 Pages
    args->threads = 0;
    args->nop_per_batch = 65536*4;
    args->clean_shared_mem = 1;
    // ...
    argp_parse(&argp, cmd_args->argc, cmd_args->argv, 0, 0, args);
    printf("args->epoch: %lu\n", args->epoch);
    printf("args->batch_size: %lu\n", args->batch_size);
    printf("args->threads: %lu\n", args->threads);
    printf("args->nop_per_batch: %lu\n", args->nop_per_batch);
    printf("args->clean_shared_mem: %lu\n", args->clean_shared_mem);

    g_performance_log = mem_map_shm(key2, NULL, sizeof(struct performance_log_entry) * NCPU, PGSIZE_4KB, false);
    printf("g_performance_log: %p\n", g_performance_log);
    if (args->clean_shared_mem) {
        memset(g_performance_log, 0, sizeof(struct performance_log_entry)*NCPU);
        ACCESS_ONCE(*g_id) = 0;
        *last_done=0;
        *g_start = rdtsc();
    }
    ACCESS_ONCE(g_meta->nop_per_batch) = args->nop_per_batch;
    ACCESS_ONCE(g_meta->batch_size) = args->batch_size;
    pthread_t threads[args->threads];
    for (int c=0; c<args->threads; c++) {
        pthread_create(&(threads[c]), NULL, bench, args);
    }
    pthread_join(threads[0], NULL);
}

int main(int argc, char *argv[])
{
    struct real_main_args *cmd_args = malloc(sizeof(*cmd_args));
    if (argc<1) {
        printf("Need the path to .config\n");
        return EINVAL;
    }
    cmd_args->argc = argc;
    cmd_args->argv = argv;
    real_main(cmd_args);
    return 0;
}
