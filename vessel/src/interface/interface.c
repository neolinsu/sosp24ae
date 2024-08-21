#include <stddef.h>
#include <pthread.h>

#include "base/cpu.h"

#include "vcontext/vcontext.h"
#include "mpk/mpk.h"

#include "vessel/interface.h"
#ifndef VESSEL_ORI
extern void __ctype_init (void);

///////////////////////////////////////////////////
// Sync with apis.h
struct vessel_alloc_chunk_api_args {
    size_t chunk_num_i;
    void*  allocated_o;
};

typedef void(*apis_func_t)(void*);
struct vessel_cluster_spawn_api_args {
    apis_func_t func_i;
    void*  args_i;
    int pref_core_id_i;
    void* ret_o;
};

typedef void(*apis_func_t)(void*);
struct vessel_spawn_all_clusters_api_args {
    apis_func_t func_i;
    void*  args_i;
    int ret_o;
};

struct vessel_register_uipi_handlers_api_args {
    void *ops_i;
    int uipi_vector_cede_fd;
    int uipi_vector_yield_fd;
    int ret_o;
};


struct vessel_create_tcb_api_args {
    tcbhead_t *ret_o;
};

struct vessel_init_tcb_api_args {
    tcbhead_t *tcb_i;
    tcbhead_t *ret_o;
};

struct vessel_dealloc_tcb_api_args {
    tcbhead_t *tcb_i;
    bool dealloc_tcb_i;
    int ret_o;
};

struct vessel_get_cpu_info_args {
    cpu_info_t *ret_o;
};

struct vessel_cluster_id_args {
    int ret_o;
};

struct vessel_cluster_to_steal_args {
    int ret_o;
};

typedef void(*api_op_func_t)(void*);
struct vessel_apis {
    api_op_func_t alloc_chunk;
    api_op_func_t alloc_huge_chunk;
    api_op_func_t cluster_spawn;
    api_op_func_t cluster_spawn_all;
    api_op_func_t cluster_yield;
    api_op_func_t cluster_exit;
    api_op_func_t create_tcb;
    api_op_func_t init_tcb;
    api_op_func_t dealloc_tcb;
    api_op_func_t register_uipi_handlers;
    api_op_func_t get_cpu_info;
    api_op_func_t cluster_mwait;
    api_op_func_t cluster_id;
    api_op_func_t cluster_to_steal;
};
typedef struct vessel_apis vessel_apis_t;

////////////////////////////////////////////////////

const vessel_apis_t *vapis = (vessel_apis_t *) 0x8d232008;
const fctx_map_t *fctx_map = (fctx_map_t *) 0x8d092000;

void *vessel_alloc_chunk(size_t chunk_num) {
    erim_switch_to_trusted;
    struct vessel_alloc_chunk_api_args ctl = {
        .chunk_num_i = chunk_num,
        .allocated_o = NULL
    };
    vapis->alloc_chunk(&ctl);
    erim_switch_to_untrusted(0);
    return ctl.allocated_o;
}

void *vessel_alloc_huge_chunk(size_t chunk_num) {
    erim_switch_to_trusted;
    struct vessel_alloc_chunk_api_args ctl = {
        .chunk_num_i = chunk_num,
        .allocated_o = NULL
    };
    vapis->alloc_huge_chunk(&ctl);
    erim_switch_to_untrusted(0);
    return ctl.allocated_o;
}

void vessel_cluster_yield(void) {
    erim_switch_to_trusted;
    vapis->cluster_yield(NULL);
    erim_switch_to_untrusted(0);
    return;
}

void vessel_cluster_exit(void) {
    erim_switch_to_trusted;
    printf("vessel_cluster_exit\n");
    vapis->cluster_exit(NULL);
    erim_switch_to_untrusted(0);
    return;
}

struct cluster_entry_args {
    vessel_func_inf_t func;
    void* args;
};

static void cluster_entry(void*args) {
    struct cluster_entry_args *meta = args;
    __ctype_init();
    //printf("the function is %p\n", meta->func);
    meta->func(meta->args);
    vessel_cluster_exit();
}

void* vessel_cluster_spawn(vessel_func_inf_t func, void* args, int pref_core_id) {
    erim_switch_to_trusted;
    struct cluster_entry_args *eargs = malloc(sizeof(struct cluster_entry_args));
    eargs->func = func;
    eargs->args = args;
    struct vessel_cluster_spawn_api_args ctl = { // TODO
        .func_i = cluster_entry,
        .args_i = (void*)eargs,
        .pref_core_id_i = pref_core_id,
        .ret_o = 0
    };
    vapis->cluster_spawn(&ctl);
    erim_switch_to_untrusted(0);
    return ctl.ret_o;
}


int vessel_cluster_spawn_all(vessel_func_inf_t func, void* args) {
    erim_switch_to_trusted;
    struct cluster_entry_args *eargs = malloc(sizeof(struct cluster_entry_args));
    eargs->func = func;
    eargs->args = args;
    struct vessel_spawn_all_clusters_api_args ctl = { // TODO
        .func_i = cluster_entry,
        .args_i = (void*)eargs,
        .ret_o = 0
    };
    vapis->cluster_spawn_all(&ctl);
    erim_switch_to_untrusted(0);
    int ret = ctl.ret_o;
    free(eargs);
    return ret;
}

tcbhead_t* vessel_create_tcb(void) {
    erim_switch_to_trusted;
    erim_switch_to_untrusted(0);
    return NULL;
}

tcbhead_t* vessel_init_tcb(tcbhead_t* allocated_tcb) {
    erim_switch_to_trusted;
    erim_switch_to_untrusted(0);
    return NULL;
}

int vessel_dealloc_tcb(tcbhead_t* allocated_tcb, bool dealloc_tcb) {
    erim_switch_to_trusted;
    erim_switch_to_untrusted(0);
    return 0;
}

int vessel_register_uipi_handlers(struct uipi_ops_inf *ops, int* uipi_vector_cede_fd_p, int* uipi_vector_yield_fd_p) {
    erim_switch_to_trusted;
    struct vessel_register_uipi_handlers_api_args *ctl = 
    malloc(sizeof(struct vessel_register_uipi_handlers_api_args));
    ctl->ops_i = ops;
    ctl->ret_o = 0;
    vapis->register_uipi_handlers(ctl);
    *uipi_vector_cede_fd_p = ctl->uipi_vector_cede_fd;
    *uipi_vector_yield_fd_p = ctl->uipi_vector_yield_fd;
    erim_switch_to_untrusted(0);
    int ret = ctl->ret_o;
    free(ctl);
    return ret;
}

cpu_info_t * vessel_get_cpu_info(void) {
    erim_switch_to_trusted;
    struct vessel_get_cpu_info_args *ctl = 
        malloc(sizeof(struct vessel_get_cpu_info_args));
    ctl->ret_o = NULL;
    vapis->get_cpu_info(ctl);
    erim_switch_to_untrusted(0);
    return ctl->ret_o;
}

void vessel_cluster_mwait(void) {
    erim_switch_to_trusted;
    vapis->cluster_mwait(NULL);
    erim_switch_to_untrusted(0);
    return;
}

int vessel_cluster_id(void) {
    erim_switch_to_trusted;
    struct vessel_cluster_id_args *ctl = 
    malloc(sizeof(struct vessel_cluster_id_args));
    vapis->cluster_id(ctl);
    erim_switch_to_untrusted(0);
    int ret = ctl->ret_o;
    free(ctl);
    return ret;
}

int vessel_cluster_to_steal(void) {
    erim_switch_to_trusted;
    static struct vessel_cluster_to_steal_args *ctl;
    if (unlikely(!ctl))
        ctl = malloc(sizeof(struct vessel_cluster_to_steal_args));
    vapis->cluster_to_steal(ctl);
    erim_switch_to_untrusted(0);
    return ctl->ret_o;
}

#else // VESSEL_ORI
#error
#include <base/mem.h>
#include <base/cpu.h>
#include <base/list.h>
#include <base/bitmap.h>
#include <base/lock.h>

#include <core/mem.h>

#include <stdlib.h>
#include "../core/mem.c"

static bool inited=false; 
static struct list_head mds;
int mpk_init(void) {
    int ret = 0;
    for (int i=1; i<16; i++) {
        ret = pkey_alloc(0, 0);
        if(ret == -1) {
            log_err("mpk_init: key error when ret = %d and i = %d.", ret, i);
        }
    }
    ret = 0;
    return ret;
}

void *vessel_alloc_chunk(size_t chunk_num) {
    assert(sizeof(chunk_num) == sizeof(uint64_t));
    assert(chunk_num!=0);
    if(unlikely(!inited)) {
        list_head_init(&mds);
        mpk_init();
        mem_init(true);
        inited = true;
    }
    void * mem = alloc_chunk(1, chunk_num, &mds);
    printf("vessel_alloc_chunk -> mem: %p\n", mem);
    assert(mem!=NULL);
    return mem;
}

void *vessel_alloc_huge_chunk(size_t chunk_num) {
    assert(sizeof(chunk_num) == sizeof(uint64_t));
    assert(chunk_num!=0);
    if(unlikely(!inited)) {
        list_head_init(&mds);
        mpk_init();
        mem_init(true);
        inited = true;
    }
    void* mem = alloc_huge_chunk(1, chunk_num, &mds);
    printf("vessel_alloc_huge_chunk -> mem: %p\n", mem);

    assert(mem!=NULL);
    return mem;
}

void vessel_cLuster_yield(void) {
    pthread_yield();
    return;
}

void vessel_cluster_exit(void) {
    pthread_exit(NULL);
    return;
}

struct cluster_entry_args {
    vessel_func_inf_t func;
    int cpuid;
    void* args;
};

static void *cluster_entry(void*args) {
    struct cluster_entry_args *meta = args;
    cpu_set_t cpuset;
    pthread_t thread;
    int ret;
    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(meta->cpuid, &cpuset);

    ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    // printf("after pthread_setaffinity_np with %s\n", strerror(errno));
    
    assert(ret==0);

    meta->func(meta->args);
    free(meta);
    pthread_exit(NULL);
}

void* vessel_cluster_spawn(vessel_func_inf_t func, void* args, int pref_core_id) {
    int ret;
    pthread_t p;
    struct cluster_entry_args myargs = {
        .func = func,
        .cpuid = pref_core_id,
        .args = args
    };
    ret = pthread_create(&p, NULL, cluster_entry, &myargs);
    return ret;
}

int vessel_cluster_spawn_all(vessel_func_inf_t func, void* args) {
    assert(bitmap_popcount(cpu_info_tbl.numa_mask, NCPU)!=0);
    int cpuid=0, ret;
    int cur_cpu = get_cur_cpuid();
    DEFINE_BITMAP(now_cpus, NCPU);
    bitmap_init(now_cpus, NCPU, false);

    bitmap_set(now_cpus,10);
    bitmap_set(now_cpus,11);
    bitmap_set(now_cpus,12);
    bitmap_set(now_cpus,13);
    bitmap_set(now_cpus,14);
    bitmap_set(now_cpus,15);

    bitmap_for_each_set(cpu_info_tbl.cpu_mask, NCPU, cpuid) {
        if(cpuid == cur_cpu) continue;
        pthread_t p;
        struct cluster_entry_args *myargs = malloc(sizeof(struct cluster_entry_args));
        myargs->func = func;
        myargs->cpuid = cpuid;
        myargs->args = args;
        ret = pthread_create(&p, NULL, cluster_entry, myargs);
        
        if(ret) {
            printf("ERROR to pthread_create\n");
            //assert(false);
            return ret;
        }
    }
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(cur_cpu, &cpuset);

    ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    return ret;
}

int vessel_register_uipi_handlers(struct uipi_ops_inf *ops) {
    return 0;
}


cpu_info_t * vessel_get_cpu_info(void) {
    cpu_init();
    return &(cpu_info_tbl);
}
#endif
////////////////////////////////////////////////////