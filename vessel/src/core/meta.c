#include <stdio.h>
#include <string.h>

#include <base/stddef.h>
#include <base/cpu.h>
#include <base/limits.h>
#include <base/assert.h>
#include <base/log.h>
#include <core/mem.h>
#include <core/task.h>
#include <core/kthread.h>
#include <core/csched.h>
#include <core/cluster.h>
#include <vessel/interface.h>

#include <mpk/mpk.h>
#include <vcontext/vcontext.h>
#include <apis/apis.h>

struct vessel_mem_meta;
extern void * __vessel_mem_start_ptr;


const static void* const_cpu_pkru_map = (void*) (VESSEL_MEM_GLOBAL_START + UPPER_ALIGN(
                                    sizeof(vessel_mem_meta_t) +
                                    sizeof(cpu_state_map_t) +
                                    sizeof(task_map_t) +
                                    sizeof(kthread_meta_map_t) +
                                    sizeof(cluster_ctx_map_t)));

struct vessel_meta {
    cpu_state_map_t    cpu_state_map;
    task_map_t         task_map;
    kthread_meta_map_t kthread_meta_map;
    cluster_ctx_map_t  cluster_ctx_map;
    uint8_t            pad1[PADSIZE(sizeof(vessel_mem_meta_t) +
                                 sizeof(cpu_state_map_t) +
                                 sizeof(task_map_t) +
                                 sizeof(kthread_meta_map_t) +
                                 sizeof(cluster_ctx_map_t)
                                )
                        ];
    cpu_pkru_map_t    cpu_pkru_map;
    kthread_fs_map_t  kthread_fs_map;
    minimal_ops_map_t minimal_ops_map;
    fctx_map_t        fctx_map; 
    uint8_t          pad2[PADSIZE(sizeof(cpu_pkru_map_t) +
                                sizeof(kthread_fs_map_t) +
                                sizeof(minimal_ops_map_t) + 
                                sizeof(fctx_map_t))];
    register_tls_ops_func_t reg_tls_ops;
    vessel_apis_t    vessel_apis;
};


typedef struct vessel_meta vessel_meta_t;

// VESSEL Inside
vessel_meta_t      *global_vessel_meta=NULL;
cpu_state_map_t    *global_cpu_state_map=NULL;
task_map_t         *global_task_map=NULL;
kthread_meta_map_t *global_kthread_meta_map=NULL;
cluster_ctx_map_t  *global_cluster_ctx_map=NULL;

// VESSEL to extension: ld.so, etc.
minimal_ops_map_t     *global_minimal_ops_map=NULL;

// VESSEL to user 
cpu_pkru_map_t    *global_cpu_pkru_map=NULL;
kthread_fs_map_t  *global_kthread_fs_map=NULL;
vessel_apis_t     *global_vessel_apis=NULL;
register_tls_ops_func_t *global_register_tls_ops_func=NULL;
fctx_map_t *global_fctx_map=NULL;
// 


BUILD_ASSERT(sizeof(vessel_meta_t) + sizeof(struct vessel_mem_meta) < VESSEL_MEM_META_SIZE);

int meta_init() {
    int ret = 0;
    log_debug("__vessel_mem_start_ptr:%p, size:%lx", __vessel_mem_start_ptr, sizeof(struct vessel_mem_meta));
    global_vessel_meta = __vessel_mem_start_ptr + sizeof(struct vessel_mem_meta);
    log_debug("global_vessel_meta:%p, size:%lx", global_vessel_meta, sizeof(vessel_meta_t));
    global_cpu_state_map = &(global_vessel_meta->cpu_state_map);
    log_debug("global_cpu_state_map:%p, size:%lx", global_cpu_state_map, sizeof(cpu_state_map_t));

    global_task_map = &(global_vessel_meta->task_map);
    global_kthread_meta_map = &(global_vessel_meta->kthread_meta_map);
    global_cluster_ctx_map = &(global_vessel_meta->cluster_ctx_map);

    global_cpu_pkru_map = &(global_vessel_meta->cpu_pkru_map);
    global_kthread_fs_map = &(global_vessel_meta->kthread_fs_map);
    global_vessel_apis = &(global_vessel_meta->vessel_apis);
    global_minimal_ops_map = &(global_vessel_meta->minimal_ops_map);
    global_register_tls_ops_func = &(global_vessel_meta->reg_tls_ops);
    global_fctx_map = &(global_vessel_meta->fctx_map);
    log_info("global_vessel_apis: %p", global_vessel_apis);
    log_info("global_minimal_ops_map: %p", global_minimal_ops_map);
    log_info("global_register_tls_ops_func: %p\n register_tls_ops_func:%p",
                global_register_tls_ops_func, *global_register_tls_ops_func);
    log_info("global_fctx_map: %p", global_fctx_map);
    
    if(const_cpu_pkru_map != (void*) global_cpu_pkru_map) {
        log_crit("ERROR unmatchced const_cpu_pkru_map: %p, global_cpu_pkru_map: %p.", const_cpu_pkru_map, (void*) global_cpu_pkru_map);
    }
    ret = pkey_mprotect((void*) __vessel_mem_start_ptr, UPPER_ALIGN(sizeof(global_vessel_meta) +  sizeof(struct vessel_mem_meta)),
                        PROT_READ | PROT_WRITE,
                        ERIM_SAFE_PKEY);
    if (ret) {
        log_emerg("Failed to pkey_mprotect __vessel_mem_start_ptr, for %s.", 
                    strerror(errno));
        return ret;
    }
    log_info("global_cpu_pkru_map: %p", global_cpu_pkru_map);
    ret = pkey_mprotect((void*) global_cpu_pkru_map,
                        UPPER_ALIGN(sizeof(cpu_pkru_map_t) +
                                    sizeof(global_kthread_fs_map) + 
                                    sizeof(minimal_ops_map_t) + 
                                    sizeof(fctx_map_t) ),
                        PROT_READ | PROT_WRITE, 
                        ERIM_TUNNEL_PKEY);
    if (ret) {
        log_emerg("Failed to pkey_mprotect global_cpu_pkru_map, for %s.", 
                    strerror(errno));
        return ret;
    }
    //ret = pkey_mprotect((void*) global_kthread_fs_map, UPPER_ALIGN(sizeof(global_kthread_fs_map)), PROT_READ | PROT_WRITE, ERIM_TUNNEL_PKEY);
    //if (ret) {
    //    log_emerg("Failed to pkey_mprotect global_kthread_fs_map, for %s.", 
    //                strerror(errno));
    //    return ret;
    //}
    return 0;
}

int rebuild_meta() { //TODO
    log_debug("Rebuild meta.");
    memset(global_vessel_meta, 0, sizeof(vessel_meta_t));
    return 0;
}