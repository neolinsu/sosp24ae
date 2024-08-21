#define _GNU_SOURCE
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <emmintrin.h>

#include <base/lock.h>
#include <base/cpu.h>
#include <base/log.h>
#include <base/assert.h>
#include <runtime/runtime.h>
#include <runtime/thread.h>
#include <runtime/simple_sync.h>
#include <runtime/tls/thread.h>

uint64_t clock;
#define MAX_TO_ESCAPE 1llu<<32


struct signal {
    atomic_t cnt;
    atomic_t out;
};

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
    // ...
    { 0 }
};


struct arguments {
    uint64_t epoch;
    uint64_t batch_size;
    uint64_t threads;
    uint64_t nop_per_batch;
    // ...
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
    _mm_stream_si128((__m128i *)buf     , a);
    _mm_stream_si128((__m128i *)(buf+16), a);
    _mm_stream_si128((__m128i *)(buf+32), a);
    _mm_stream_si128((__m128i *)(buf+48), a);
}

struct performance_log_entry {
    uint64_t done;
    char pad[CACHE_LINE_SIZE - sizeof(uint64_t)];
};
struct performance_log_entry g_performance_log[NCPU];

struct signal g_epoch_start_signal;
struct signal g_epoch_end_signal;

struct tls {
    int id;
};
atomic_t g_id;
uint64_t last_done;
uint64_t g_start;

void bench(void * a) {
    struct tls *tls = malloc(sizeof(*tls));
    tls->id = atomic_fetch_and_add(&g_id, 1);
    set_uthread_specific((uint64_t) tls);
    struct arguments *args = a;
    uint64_t epoch = args->epoch,
        batch_size = args->batch_size,
        nop_per_batch = args->nop_per_batch;
    char* buf = aligned_alloc(CACHE_LINE_SIZE, batch_size * CACHE_LINE_SIZE);
    memset(buf, -1, batch_size * CACHE_LINE_SIZE);
    for (int c=0; c<CACHE_LINE_SIZE*batch_size; c+=CACHE_LINE_SIZE) {
        _mm_clflush(buf+c);
    }
    _mm_mfence();
    uint64_t rand_state = rdtsc();
    int c = (int) rand_state;
    // uint64_t range_mask = batch_size - 1;
    for (uint64_t escape=0; escape<MAX_TO_ESCAPE; ++escape) {
        for (uint64_t epoch_i=0; epoch_i<epoch; ++epoch_i) {
            for (uint64_t batch_i=0; batch_i<batch_size; ++batch_i) {
                char *ptr = buf + batch_i * 64;
                nt_store(ptr, c);
            }
            _mm_mfence();
            g_performance_log[tls->id].done++;
            for(uint64_t nop_i=0; nop_i<nop_per_batch; ++nop_i) {
                __asm volatile ("nop");
            }
            thread_yield();
        }
    }
}

void old_bench(void * a) {
    struct tls *tls = malloc(sizeof(*tls));
    tls->id = atomic_fetch_and_add(&g_id, 1);
    set_uthread_specific((uint64_t) tls);
    struct arguments *args = a;
    uint64_t epoch = args->epoch,
        batch_size = args->batch_size,
        nop_per_batch = args->nop_per_batch,
        threads = args->threads;
    char* buf = aligned_alloc(CACHE_LINE_SIZE, batch_size * CACHE_LINE_SIZE);
    memset(buf, -1, batch_size * CACHE_LINE_SIZE);
    for (int c=0; c<CACHE_LINE_SIZE*batch_size; c+=CACHE_LINE_SIZE) {
        _mm_clflush(buf+c);
    }
    _mm_mfence();
    uint64_t rand_state = rdtsc();
    int c = (int) rand_state;
    uint64_t epoch_size = batch_size * CACHE_LINE_SIZE * epoch * threads;
    // uint64_t range_mask = batch_size - 1;
    for (uint64_t escape=0; escape<MAX_TO_ESCAPE; ++escape) {
        bool is_start = signal_wait(&g_epoch_start_signal);
        if (is_start) {
            g_start = rdtsc();
            signal_init(&g_epoch_start_signal, threads);
        }
        for (uint64_t epoch_i=0; epoch_i<epoch; ++epoch_i) {
            for (uint64_t batch_i=0; batch_i<batch_size; ++batch_i) {
                char *ptr = buf + batch_i * 64;
                nt_store(ptr, c);
            }
            _mm_mfence();
            for(uint64_t nop_i=0; nop_i<nop_per_batch; ++nop_i) {
                __asm volatile ("nop");
            }
        }
        bool is_end = signal_wait(&g_epoch_end_signal);
        if (is_end) {
            uint64_t lasting = rdtsc() - g_start;
            signal_init(&g_epoch_end_signal, threads);
            double time = (double) lasting;
            time /= ((double)cycles_per_us*1000*1000);
            printf("{\"bandwith\": \" %lf \"}", ((double) epoch_size) / (time * 1024*1024));
        }
    }
}

void perf_timer(void * a) {
    struct arguments *args = a;
    uint64_t batch_size = args->batch_size,
        threads = args->threads;
    uint64_t batch_bytes = batch_size * CACHE_LINE_SIZE;
    while(true) {
        usleep(5*1000*1000);
        uint64_t done = 0;
        uint64_t cur = rdtsc();
        for (int t=0; t<threads; t++) {
            done += g_performance_log[t].done;
        }
        done -= last_done;
        uint64_t lasting = cur - g_start;
        g_start = rdtsc();
        double time = (double) lasting;
        time /= ((double)cycles_per_us*1000*1000);
        vessel_write_log("{\"bandwidth\": %lf }", ((double) batch_bytes * done) / (time * 1024*1024));
        //printf("{\"bandwidth\": %lf } RES_END\n", ((double) batch_bytes * done) / (time * 1024*1024));
        last_done += done;
    }
}

struct real_main_args {
    int argc;
    void* argv;
};

void real_main(void* a) {
    struct real_main_args *cmd_args = a;
    struct arguments *args = malloc(sizeof(*args));

    args->epoch = 8192;
    args->batch_size = 65536; //1024 Pages
    args->threads = 0;
    args->nop_per_batch = 65536*5;
    // ...
    argp_parse(&argp, cmd_args->argc, cmd_args->argv, 0, 0, args);
    printf("args->epoch: %lu\n", args->epoch);
    printf("args->batch_size: %lu\n", args->batch_size);
    if (args->threads==0) args->threads = maxks - 2;
    printf("args->threads: %lu\n", args->threads);
    printf("args->nop_per_batch: %lu\n", args->nop_per_batch);

    memset(g_performance_log, 0, sizeof(struct performance_log_entry)*NCPU);
    atomic_write(&g_id, 0);
    last_done=0;
    signal_init(&g_epoch_start_signal, args->threads);
    signal_init(&g_epoch_end_signal, args->threads);
    g_start = rdtsc();
    for (int c=0; c<args->threads; c++) {
        thread_spawn(bench, args);
    }
    perf_timer(args);
}

int main(int argc, char *argv[])
{
    struct real_main_args *cmd_args = malloc(sizeof(*cmd_args));
    if (argc<1) {
        printf("Need the path to .config\n");
        return EINVAL;
    }
    cmd_args->argc = argc-1;
    cmd_args->argv = &argv[1];
    runtime_init(argv[1], real_main, cmd_args);
    return 0;
}