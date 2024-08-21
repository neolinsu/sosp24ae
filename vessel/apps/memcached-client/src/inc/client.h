#pragma once
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include <rand.h>
#include <memcached_meta.h>
#include <mt19937_64.h>

#include <base/assert.h>

struct req {
    uint64_t target_start_tsc;
    uint64_t real_start_tsc;
    uint64_t real_end_tsc;
    void* packet;
    size_t len;
};

struct signal {
    atomic_t cnt;
    atomic_t out;
};

struct client {
    uint64_t preload_req_num;
    uint64_t warm_up_req_num;
    uint64_t req_num;
    struct req *preload_list;
    struct req *warm_up_list;
    struct req *list;
    struct barrier* start_barrier;
    struct barrier* preload_barrier;
    struct barrier* warm_up_barrier;
    struct barrier* begin_barrier;
    struct barrier* end_barrier;
    uint32_t client_num;
    struct signal *start_sig;
    struct signal *end_sig;
    bool async;
    enum ConnectType conn_type;
    void* conn_args;
    spinlock_t l;
    uint64_t *work_start_p;
    struct memcached_meta* meta;
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
                while(atomic_read(&sig->out)>0)
                    cpu_relax();
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

typedef uint64_t (*uint64_gen_func_t)(void*);
typedef uint64_gen_func_t target_start_gen_func_t;
typedef uint64_gen_func_t key_gen_func_t;
typedef uint8_t (*uint8_gen_func_t)(void*);
typedef uint8_gen_func_t opcode_gen_func_t;

struct req** create_reqs(
    uint64_t req_num,
    uint64_t client_num,
    target_start_gen_func_t target_start_gen_func,
    key_gen_func_t key_gen_func,
    opcode_gen_func_t op_gen_func,
    struct memcached_meta* m);

void client_thread(void* args);

struct client_tls {
    uint64_gen_func_t _u64_gen_func;
    uint8_gen_func_t  _u8_gen_func;
    void* thread_conn;
    conn_op_func_t thread_conn_try_write_op;
    conn_op_func_t thread_conn_try_read_op;
};
