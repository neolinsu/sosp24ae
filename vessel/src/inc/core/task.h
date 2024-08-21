#pragma once
#include <stddef.h>
#include <unistd.h>

#include "base/bitmap.h"
#include "base/lock.h"
#include "base/list.h"
#include "base/cpu.h"
#include "base/limits.h"
#include "base/tcb.h"

#include "core/cluster.h"

typedef int task_id_t;

// _dl_allocate_tls_storage
// _dl_allocate_tls_init
// _dl_deallocate_tls

struct tls_ops {
    allocate_tls_storage_t allocate_tls_storage;
    tls_init_t tls_init;
    deallocate_tls_t deallocate_tls;
};

struct task {
    int active;
    task_id_t task_id;
    pid_t pid;
    int mpk;
    cpu_info_t cpu_info;
    spinlock_t mds_lock;
    struct list_head mds;
    spinlock_t cluster_ctx_lock;
    struct list_head cluster_ctx; // TODO: Done by init.
    spinlock_t tls_ops_lock;
    struct tls_ops tls_ops;
    spinlock_t miniop_l;
    atomic64_t miniop_cnt;
    uint64_t   miniop_base;
    uint64_t   miniop_last;
};
typedef struct task task_t;

struct task_map {
    spinlock_t lock;
    task_id_t anchor;
    size_t task_num;
    task_t map[NTASK];
};
typedef struct task_map task_map_t;

struct task_start_args {
    char*  path;
    char**  env;
    int    argc;
    char** argv;
    uid_t   uid;
};
typedef struct task_start_args task_start_args_t;


extern task_t* task_alloc(void);
extern cluster_ctx_t* build_task_start_cluster(task_t *task, task_start_args_t* argv);
extern int task_attach_cluster(task_t *task, cluster_ctx_t* cluster);
extern int task_deattach_cluster(cluster_ctx_t* cluster);

extern void* startup_vessel_alloc_task_chunk(size_t chunk_num);
extern void startup_vessel_register_tls_ops(struct tls_ops *ops);

typedef void(*register_tls_ops_func_t)(struct tls_ops*);

extern int task_meta_init(void);