#pragma once

#include <stdint.h>
#include <pthread.h>
#include <immintrin.h>

#include "base/limits.h"
#include "base/list.h"
#include "base/lock.h"
#include "base/bitmap.h"
#include "base/cpu.h"

#include "core/cluster.h"
#include "core/task.h"


struct kthread_meta {
    unsigned int cpuid;
    uint64_t kthread_fs;
    spinlock_t cluster_list_lock;
    struct list_head cluster_list;
};

typedef struct kthread_meta kthread_meta_t;

struct kthread_meta_map {
    spinlock_t lock;
    DEFINE_BITMAP(map, NCPU);
    kthread_meta_t  kthread_meta[NCPU];
};
typedef struct kthread_meta_map kthread_meta_map_t;

struct kthread_init_args {
    kthread_meta_t *kthread_meta;
};
typedef struct kthread_init_args kthread_init_args_t;

struct kthread_fs_map {
    uint64_t kthread_fs[NCPU];
};
typedef struct kthread_fs_map kthread_fs_map_t;

extern int kthread_alloc(task_t *cur_task);
extern int kthread_alloc_bitmap(bitmap_ptr_t map);
extern int kthread_attch_cluster(kthread_meta_t *kthread_meta, cluster_ctx_t *cluster);
extern int kthread_attch_cluster_head(kthread_meta_t *kthread_meta, cluster_ctx_t *cluster);
extern int kthread_deattch_cluster(cluster_ctx_t *cluster);
extern int kthread_park_cluster(cluster_ctx_t *cluster);
extern cluster_ctx_t * kthread_pop_cluster(kthread_meta_t * kthread_meta);

extern __thread kthread_meta_t *my_kthread_meta;
extern __thread char *my_kthread_stack;
extern __thread void *cur_cluster;
extern kthread_fs_map_t *global_kthread_fs_map;
extern kthread_meta_map_t * global_kthread_meta_map;


static inline void switch_to_kthread_fs() {
    loadfs(global_kthread_fs_map->kthread_fs[get_cur_cpuid()]);
};

static __always_inline void switch_to_stack(uint64_t *stack) {
    asm volatile("movq %0, %%rsp" : :"r" (*stack));
};

static __always_inline void save_to_stack(uint64_t *stack) {
    asm volatile("movq %%rsp, %0" : "=r" (*stack));
};

static inline void get_pkru(int *pkru)
{
        asm volatile(
            "xor %%ecx, %%ecx\n\t"
            "RDPKRU\n\t"
		     "mov %%eax, %0" :"=r"(*pkru));
};


static inline kthread_meta_t * get_kthread_meta_from_cpuid(int cpuid) {
    return &(global_kthread_meta_map->kthread_meta[cpuid]);
};

extern int kthread_meta_init(void);