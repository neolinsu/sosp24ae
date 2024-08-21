#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "base/limits.h"
#include "base/bitmap.h"
#include "base/time.h"
#include "base/cpu.h"
#include "base/log.h"

#include "core/task.h"
#include "core/kthread.h"
#include "core/uipi.h"
#include "core/mem.h"
#include "core/pmc.h"
#include "core/csched.h"

#include "mpk/mpk.h"


extern kthread_meta_map_t *global_kthread_meta_map;


__thread kthread_meta_t *my_kthread_meta=NULL;
__thread char *my_kthread_stack=NULL;
__thread void *cur_cluster=NULL;

spinlock_t cluster_id2kthread_ptr_l;
void* cluster_id2kthread_ptr[NCLUSTER];

// extern void vthread_init(void*arg);
void kpanic(void) {
    log_err("kpanic");
    abort();
}

void kthread_sched() { // TODO: ksched
    // log_info("Start to kthread_sched()");
    // save_to_stack(&my_kthread_stack);
    // log_info("my_kthread_stack:%p", my_kthread_stack);
    while(true) {
        cluster_yield(); // to csched.
        abort();
    }
}
static pthread_barrier_t init_barrier;

void kthread_init(void *args) {
    int ret;
    kthread_init_args_t *myargs = (kthread_init_args_t *) args;
    kthread_meta_t *meta = myargs->kthread_meta;
    if (meta->kthread_fs != get_cur_fs()) {
        log_crit("kthread_fs(%lu) can not match the get_cur_fs(%lu).", meta->kthread_fs, get_cur_fs());
        goto panic;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(meta->cpuid, &cpuset);
    ret = pthread_setaffinity_np(meta->kthread_fs, sizeof(cpuset), &cpuset);
    if (ret) {
        log_crit("Fail to init pthread(%lu) on cpu(%d), for %s", meta->kthread_fs, get_cur_cpuid(), strerror(errno));
        goto panic;
    }
    
    ACCESS_ONCE(my_kthread_meta) = meta;
    ACCESS_ONCE(global_kthread_fs_map->kthread_fs[meta->cpuid]) = meta->kthread_fs;
    ACCESS_ONCE(my_kthread_stack) = (((char*) aligned_alloc(16, 256*KB)) + 256*KB);
    *((uint64_t*)(my_kthread_stack - 8)) = (uint64_t) kpanic;
    my_kthread_stack-=8;
    // TODO
    ret = register_uipi_handlers();
    if (ret) {
         goto panic;
    }
    ret = pthread_detach(meta->kthread_fs);
    BUG_ON(ret);
    ret = pmc_init_perthread();
    BUG_ON(ret);
redo:
        free(myargs);
        
	    pthread_barrier_wait(&init_barrier);

        kthread_sched(); // TODO: Schedule.
        log_err("Unexpected return from kthread_sched!");
        goto redo;
panic:
    log_err("PANIC!"); while(1);
    return;
}

int kthread_alloc(task_t *new_task) {
    int ret, i;
    spin_lock(&(global_kthread_meta_map->lock));
    DEFINE_BITMAP(tmp_map, NCPU);
    bitmap_init(tmp_map, NCPU, false);
    bitmap_xor(tmp_map, new_task->cpu_info.cpu_mask, global_kthread_meta_map->map, NCPU);
    bitmap_and(tmp_map, new_task->cpu_info.cpu_mask, tmp_map, NCPU);

	pthread_barrier_init(&init_barrier, NULL, bitmap_popcount(tmp_map, NCPU) + 1);
    bitmap_for_each_set(tmp_map, NCPU, i) {
        kthread_init_args_t *myargs = malloc(sizeof(kthread_init_args_t));
        myargs->kthread_meta = &(global_kthread_meta_map->kthread_meta[i]);
        kthread_meta_t *meta = myargs->kthread_meta;
        list_head_init(&(meta->cluster_list));
        spin_lock_init(&(meta->cluster_list_lock));
        meta->cpuid = (unsigned int)i;
        ret = pthread_create(&(meta->kthread_fs), NULL, (void*) kthread_init, (void*) myargs);
        if (ret) {
            log_crit("Fail to allocate kthread.");
            goto out;
        }
        bitmap_set(global_kthread_meta_map->map, i);
    }
    ret = 0;
out:
    spin_unlock(&(global_kthread_meta_map->lock));
    pthread_barrier_wait(&init_barrier);

    return ret;
}

int kthread_alloc_bitmap(bitmap_ptr_t map) {
    int ret, i;
    spin_lock(&(global_kthread_meta_map->lock));
    DEFINE_BITMAP(tmp_map, NCPU);
    bitmap_init(tmp_map, NCPU, false);
    bitmap_xor(tmp_map, map, global_kthread_meta_map->map, NCPU);
    bitmap_and(tmp_map, map, tmp_map, NCPU);

	pthread_barrier_init(&init_barrier, NULL, bitmap_popcount(tmp_map, NCPU) + 1);
    bitmap_for_each_set(tmp_map, NCPU, i) {
        kthread_init_args_t *myargs = malloc(sizeof(kthread_init_args_t));
        myargs->kthread_meta = &(global_kthread_meta_map->kthread_meta[i]);
        kthread_meta_t *meta = myargs->kthread_meta;
        list_head_init(&(meta->cluster_list));
        spin_lock_init(&(meta->cluster_list_lock));
        meta->cpuid = (unsigned int)i;
        ret = pthread_create(&(meta->kthread_fs), NULL, (void*) kthread_init, (void*) myargs);
        if (ret) {
            log_crit("Fail to allocate kthread.");
            goto out;
        }
        bitmap_set(global_kthread_meta_map->map, i);
    }
    ret = 0;
out:
    spin_unlock(&(global_kthread_meta_map->lock));
    pthread_barrier_wait(&init_barrier);

    return ret;
}

int kthread_attch_cluster(kthread_meta_t *kthread_meta, cluster_ctx_t *cluster) {
    int ret = 0;
    spin_lock(&cluster_id2kthread_ptr_l);
    
    ret = spin_try_lock(&kthread_meta->cluster_list_lock);
    if (!ret) {
        spin_unlock(&cluster_id2kthread_ptr_l);
        return -EAGAIN;
    }
    list_add_tail(&kthread_meta->cluster_list, &cluster->kthread_link);
    spin_unlock(&kthread_meta->cluster_list_lock);

    ACCESS_ONCE(cluster_id2kthread_ptr[cluster->id]) = kthread_meta;
    spin_unlock(&cluster_id2kthread_ptr_l);

    ret = 0;
    return ret;
}

int kthread_park_cluster(cluster_ctx_t *cluster) {
    int ret = 0;
 
    spin_lock(&cluster_id2kthread_ptr_l);
    ACCESS_ONCE(cluster_id2kthread_ptr[cluster->id]) = NULL;
    spin_unlock(&cluster_id2kthread_ptr_l);

    ret = 0;
    return ret;
}

int kthread_attch_cluster_head(kthread_meta_t *kthread_meta, cluster_ctx_t *cluster) {
    int ret = 0;
    spin_lock(&cluster_id2kthread_ptr_l);

    ret = spin_try_lock(&kthread_meta->cluster_list_lock);
    if (!ret) {
        spin_unlock(&cluster_id2kthread_ptr_l);
        return -EAGAIN;
    }
    list_add(&kthread_meta->cluster_list, &cluster->kthread_link);
    spin_unlock(&kthread_meta->cluster_list_lock);

    ACCESS_ONCE(cluster_id2kthread_ptr[cluster->id]) = kthread_meta;
    spin_unlock(&cluster_id2kthread_ptr_l);

    ret = 0;
    return ret;
}

cluster_ctx_t * kthread_pop_cluster(kthread_meta_t * kthread_meta) {
    int ret = 0;
    cluster_ctx_t * res;
    spin_lock(&cluster_id2kthread_ptr_l);

    ret = spin_try_lock(&kthread_meta->cluster_list_lock);
    if (!ret) {
        spin_unlock(&cluster_id2kthread_ptr_l);
        return NULL;
    }
    res = list_pop(&kthread_meta->cluster_list, cluster_ctx_t, kthread_link);
    spin_unlock(&kthread_meta->cluster_list_lock);
    assert(res == NULL || kthread_meta == cluster_id2kthread_ptr[res->id]);
    if (likely(res != NULL))
        ACCESS_ONCE(cluster_id2kthread_ptr[res->id]) = NULL;
    spin_unlock(&cluster_id2kthread_ptr_l);

    return res;
}

int kthread_deattch_cluster(cluster_ctx_t *cluster) {
    int ret = 0;
    spin_lock(&cluster_id2kthread_ptr_l);
    kthread_meta_t *kthread_meta = cluster_id2kthread_ptr[cluster->id];
    if (!kthread_meta) {
        ret = 0;
        spin_unlock(&cluster_id2kthread_ptr_l);
        return ret;
    }

    ret = spin_try_lock(&kthread_meta->cluster_list_lock);
    if (!ret) {
        spin_unlock(&cluster_id2kthread_ptr_l);
        return -EAGAIN;
    }
    list_del_from(&kthread_meta->cluster_list, &cluster->kthread_link);    
    spin_unlock(&kthread_meta->cluster_list_lock);

    ACCESS_ONCE(cluster_id2kthread_ptr[cluster->id]) = NULL;
    spin_unlock(&cluster_id2kthread_ptr_l);
    return ret;
}

int kthread_meta_init(void) {
    memset(cluster_id2kthread_ptr, 0, sizeof(cluster_id2kthread_ptr));
    spin_lock_init(&cluster_id2kthread_ptr_l);
    return 0;
}