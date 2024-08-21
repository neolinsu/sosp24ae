#define _GNU_SOURCE
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net_utils.h>
#include <rand.h>
#include <client.h>
#include <gen.h>
#include <stat.h>

#include <base/lock.h>
#include <base/log.h>
#include <base/assert.h>
#include <runtime/runtime.h>
#include <runtime/thread.h>
#include <runtime/simple_sync.h>
#include <runtime/tls/thread.h>

uint64_t clock;

const char *argp_program_version = "memcached-client (0.1)";
const char *argp_program_bug_address = "-";
static char doc[] = "Record the addresses of the targeted event base on perf_event_open. X86 only for now.";
static struct argp_option options[] = {
    { "nvalues", 'n', "NVALUES", 0, "(30000)."},
    { "clock", 'c', "CYCLES", 0, "(2500)."},
    { "protocol_type", 'p', "PROTOTYPE", 0, "[tcp, cxltcp]: cxltp."},
    { "bandwidth", 'b', "BANDWIDTH", 0, "(1m/s)."},
    { "distribution", 'd', "DIST", 0, "[exp]: exp."},
    { "warm_up", 'w', "WARMUP", 0, "(10000)."},
    { "times", 't', "TIMES", 0, "(20s)."},
    { "value_size", 'v', "SVALUE", 0, "(2 Byte)."},
    { "key_size", 'k', "SKEY", 0, "(20 Byte)."},
    { "cxltp_base", 'C', "CXLTPBASE", 0, "-."},
    { "ip", 'S', "SEVERIP", 0, "(0)."},
    { "port", 'P', "PORT", 0, "(0)."},
    // ...
    { 0 }
};

struct arguments {
    uint64_t nvalues;
    uint64_t clock;
    int protocol_type;
    double bandwidth;
    int distribution_type;
    uint64_t warm_up;
    uint64_t times;
    uint64_t value_size;
    uint64_t key_size;
    void* cxltp_base;
    struct netaddr saddr;
    uint64_t port;
    // ...
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    int ret;
    switch (key) {
    case 'n':
        sscanf(arg, "%lu", &args->nvalues);
        printf("nvalues: %lu\n", args->nvalues);
    break;
    case 'p':
        if (0==strcmp(arg, "cxltp")) {
            args->protocol_type = CXLTP;
        } else if (0==strcmp(arg, "tcp")) {
            args->protocol_type = TCP;
        } else return EINVAL;
    break;
    case 'b': sscanf(arg, "%lf", &args->bandwidth);break;
    case 'd': {
        int d;
        sscanf(arg, "%d", &d);
        if (arg==Exponential) {
            args->distribution_type = Exponential;
        } else return EINVAL;
    }
    break;
    case 'w': sscanf(arg, "%lu", &args->warm_up); break;
    case 't': sscanf(arg, "%lu", &args->times); break;
    case 'v': sscanf(arg, "%lu", &args->value_size); break;
    case 'k': sscanf(arg, "%lu", &args->key_size); break;
    case 'C': sscanf(arg, "%lx", (uint64_t*)&args->cxltp_base); break;
    case 'S': {
        ret = str_to_netaddr(arg, &args->saddr);
        if (ret)
            return EINVAL;
    }
    break;
    case 'P': sscanf(arg, "%lu", &args->port); break;
    // ...
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

target_start_gen_func_t get_target_start_gen_func(struct arguments *args) {
    if (args->distribution_type == Exponential) {
        set_time_gen_exponential_mean((args->bandwidth));
        return time_gen_exponential_accu;
    } else {
        printf("Unknown Distribution.\n");
        abort();
    }
}

void* get_conn_args(struct arguments *args) {
    if (args->protocol_type == CXLTP) {
        struct cxltp_args *cxltp_args = malloc(sizeof(struct cxltp_args));
        cxltp_args->base = args->cxltp_base;
        cxltp_args->port = args->port;
        return cxltp_args;
    } else if (args->protocol_type == TCP){
        args->saddr.port = args->port;
        return &args->saddr;
    } else {
        printf("Unknown Distribution.\n");
        abort();
    }
}

void client(struct arguments *args) {
    uint64_t client_num = maxks;
    printf("client_num: %lu\n", client_num);
    struct client_tls *tls = malloc(sizeof(*tls));
    set_uthread_specific((uint64_t) tls);

    struct memcached_meta *m = malloc(sizeof(struct memcached_meta));
    int ret = memcached_meta_init(m,
        args->value_size,
        args->key_size,
        args->nvalues);
    if (ret) {
        printf("Error memcached_meta_init\n");
        return;
    }

    set_key_start(0);
    struct req ** preload_lists = create_reqs(
        args->nvalues,
        client_num,
        gen_zero,
        key_gen_acc,
        op_gen_w10,
        m);

    set_key_range(args->nvalues);
    target_start_gen_func_t ts_gen_func = get_target_start_gen_func(args);
    struct req ** warm_up_lists = create_reqs(
        args->warm_up,
        client_num,
        ts_gen_func,
        key_gen_uniform,
        op_gen_w1r9,
        m);

    uint64_t nvalues = (uint64_t) ((double)args->times * 
                    args->bandwidth *
                    1000000.0);
    struct req ** lists = create_reqs(
        nvalues,
        client_num,
        ts_gen_func,
        key_gen_uniform,
        op_gen_w1r9,
        m);

    barrier_t start_barrier;
    barrier_t preload_barrier;
    barrier_t warm_up_barrier;
    barrier_t begin_barrier;
    barrier_t end_barrier;
    assert(client_num<INT32_MAX);
    barrier_init(&start_barrier, client_num);
    barrier_init(&preload_barrier, client_num);
    barrier_init(&warm_up_barrier, client_num);
    barrier_init(&begin_barrier, client_num);
    barrier_init(&end_barrier, client_num+1);
    struct signal start_sig, end_sig;
    signal_init(&start_sig, client_num);
    signal_init(&end_sig, client_num);
    uint64_t work_start;
    struct client cargs[client_num];
    for (int c=0; c<client_num; ++c) {
        cargs[c].preload_req_num = args->nvalues / client_num;
        cargs[c].warm_up_req_num = args->warm_up / client_num;
        cargs[c].req_num = nvalues / client_num;

        cargs[c].preload_list = preload_lists[c];
        cargs[c].warm_up_list = warm_up_lists[c];
        cargs[c].list = lists[c];

        cargs[c].start_barrier = &start_barrier;
        cargs[c].preload_barrier = &preload_barrier;
        cargs[c].warm_up_barrier = &warm_up_barrier;
        cargs[c].begin_barrier = &begin_barrier;
        cargs[c].end_barrier = &end_barrier;

        cargs[c].client_num = (uint32_t) client_num;
        cargs[c].start_sig  = &start_sig;
        cargs[c].end_sig    = &end_sig;

        cargs[c].async = false;
        cargs[c].conn_type = args->protocol_type;
        cargs[c].conn_args = get_conn_args(args); // TOFREE
        cargs[c].work_start_p = &work_start;
        cargs[c].meta = m;
        thread_spawn(client_thread , &cargs[c]);
    }

    barrier_wait(&end_barrier);
    printf("Work Done estime: %lf s\n", (double)(rdtsc()-work_start)/(clock*1000*1000));
    uint64_t *lat;
    get_lat(
        client_num,
        nvalues/client_num,
        lists,
        &lat);
} 

struct real_main_args {
    int argc;
    void* argv;
};

void real_main(void* a) {
    struct real_main_args *cmd_args = a;
    mt19937_64_init();
 
    struct arguments *args = malloc(sizeof(*args));

    args->nvalues = 30000;
    args->clock = 2500;
    args->protocol_type = CXLTP;
    args->distribution_type = Exponential;
    args->warm_up = 300000;
    args->times = 20;
    args->bandwidth = 1.0;
    args->value_size = 2;
    args->key_size = 20;
    args->cxltp_base = (void*) 0x2584600000;
    args->saddr.ip = 0;
    args->saddr.port = 0;
    args->port = 0;
    // ...
    argp_parse(&argp, cmd_args->argc, cmd_args->argv, 0, 0, args);
    printf("args->nvalues: %lu\n", args->nvalues);
    printf("args->bandwidth: %lf\n", args->bandwidth);
    printf("args->clock: %lu\n", args->clock);
    printf("args->protocol_type: %d"
        " (CXLTP %d, TCP %d)\n", args->protocol_type, CXLTP, TCP);
    printf("args->distribution_type: %d"
        " (Exponential %d)\n", args->distribution_type, Exponential);
    printf("args->warm_up: %lu\n", args->warm_up);
    printf("args->times: %lu\n", args->times);
    printf("args->value_size: %lu\n", args->value_size);
    printf("args->key_size: %lu\n", args->key_size);
    printf("args->cxltp_base: %p\n", args->cxltp_base);
    printf("args->server ip: %u\n", args->saddr.ip);
    printf("args->port: %lu\n", args->port);
    clock = args->clock;
    client(args);
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