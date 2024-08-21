#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "base/log.h"
#include "base/cpu.h"
#include "base/bitmap.h"

#include "core/mem.h"
#include "core/kthread.h"
#include "core/task.h"
#include "core/csched.h"
#include "core/cluster.h"

#include "apis/apis.h"


extern vessel_apis_t *global_vessel_apis;

/// @brief Alloc chunk for current task.
/// @param chunk_num: # of chunks.
/// @return void.
void vessel_alloc_chunk_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    struct vessel_alloc_chunk_api_args* meta = (struct vessel_alloc_chunk_api_args*) args;
    size_t chunk_num = meta->chunk_num_i;
    task_t *cur_task = (task_t *) ((cluster_ctx_t*)cur_cluster)->mytask;
    void *res = alloc_chunk(cur_task->task_id,
                            chunk_num, &(cur_task->mds));
    if (res == NULL) {
        log_crit("Fail to alloc_chunk, for %d (%s).", errno, strerror(errno));
    }
    meta->allocated_o = res;
    loadfs(cur_fs);
    return;
}

/// @brief Alloc huge chunk for current task.
/// @param chunk_num: # of chunks.
/// @return void.
void vessel_alloc_huge_chunk_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    struct vessel_alloc_chunk_api_args* meta = (struct vessel_alloc_chunk_api_args*) args;
    size_t chunk_num = meta->chunk_num_i;
    task_t *cur_task = (task_t *) ((cluster_ctx_t*)cur_cluster)->mytask;
    void *res = alloc_huge_chunk(cur_task->task_id,
                            chunk_num, &(cur_task->mds));
    if (res == NULL) {
        log_crit("Fail to alloc_chunk, for %d (%s).", errno, strerror(errno));
    }
    meta->allocated_o = res;
    loadfs(cur_fs);
    return;
}

/// @brief Create a new cluster.
/// @param apis_func_t func_i: entry.
/// @param void*  args_i: args.
/// @param int pref_core_id_i: preferred cpu id.
/// @return int ret_o: 0 if success.
void vessel_cluster_spawn_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    cur->fctx.FS = cur_fs;
    struct vessel_cluster_spawn_api_args* meta = (struct vessel_cluster_spawn_api_args*) args;
    meta->ret_o = cluster_spawn(meta->func_i, meta->args_i, meta->pref_core_id_i);
    loadfs(cur_fs);
    return;
}

/// @brief Spawn all clusters of the task besides the current cluster.
/// @param apis_func_t func_i: entry.
/// @param void*  args_i: args.
/// @return int ret_o: 0 if success.
void vessel_spawn_all_clusters_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t *cur = (cluster_ctx_t*) cur_cluster;
    cur->fctx.FS = cur_fs;
    task_t *task = cur->mytask;
    int cpu_id, cur_cpu = get_cur_cpuid();
    void* ret;
    struct vessel_spawn_all_clusters_api_args* meta = args;
    bitmap_for_each_set(task->cpu_info.cpu_mask, NCPU, cpu_id) {
        if(unlikely(cur_cpu == cpu_id)) continue;
        ret = cluster_spawn(meta->func_i, meta->args_i, cpu_id);
        log_info("Spawn Cluster on CPU %d", cpu_id);
        if (unlikely(!ret)) {
            meta->ret_o = EAGAIN;
            goto out;
        }
    }
    meta->ret_o = 0;
out:
    loadfs(cur_fs);
    return;
}

/// @brief Yield to next cluster.
/// @param void.
/// @return void.
void vessel_cluster_yield_api(void* args) {
    //fflush(stdout);
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_yield();
    loadfs(cur_fs);
    return;
}

/// @brief Exit this cluster and yield to next cluster.
/// @param void.
/// @return void.
void vessel_cluster_exit_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_exit();
    loadfs(cur_fs);
    return;
}

/// @brief Create a tcb.
/// @param void.
/// @return tcbhead_t* ret_o: the ptr of allocated tcb.
void vessel_create_tcb_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    struct vessel_create_tcb_api_args* meta = args;
    task_t *cur_task = (task_t *) cur->mytask;
    spin_lock(&cur_task->tls_ops_lock);
    tcbhead_t *tcbhead = ((allocate_tls_storage_t)cur_task->tls_ops.allocate_tls_storage)();
    spin_unlock(&cur_task->tls_ops_lock);
    meta->ret_o = tcbhead;
    loadfs(cur_fs);
    return;
}

/// @brief Init a tcb.
/// @param tcbhead_t* tcb_i: the ptr of allocated tcb.
/// @return tcbhead_t* tcb_i: the ptr of inited tcb(same as the tcb_i), if succeed.
void vessel_init_tcb_api(void* args) {
    //log_info("Call exit_api");
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    //save_to_stack(&cur->api_user_stack);
    //switch_to_stack(&my_kthread_stack);
    struct vessel_init_tcb_api_args* meta = args;
    task_t *cur_task = (task_t *) cur->mytask;
    spin_lock(&cur_task->tls_ops_lock);
#ifdef VESSEL_GLIBC2_34
    tcbhead_t * tcbhead = ((tls_init_t)cur_task->tls_ops.tls_init)(meta->tcb_i, true);
#else
    tcbhead_t * tcbhead = ((tls_init_t)cur_task->tls_ops.tls_init)(meta->tcb_i);
#endif
    spin_unlock(&cur_task->tls_ops_lock);
    meta->ret_o = tcbhead;
    //cluster_ctx_t* cur2 = ACCESS_ONCE(cur_cluster);
    //switch_to_stack(&cur2->api_user_stack);
    loadfs(cur_fs);
    return;
}

/// @brief Deallocated a tcb.
/// @param tcbhead_t* tcb_i: the ptr of allocated tcb.
/// @return void.
void vessel_dealloc_tcb_api(void* args) {
    //log_info("Call exit_api");
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    //save_to_stack(&cur->api_user_stack);
    //switch_to_stack(&my_kthread_stack);
    struct vessel_dealloc_tcb_api_args* meta = args;
    task_t *cur_task = (task_t *) cur->mytask;
    spin_lock(&cur_task->tls_ops_lock);
    ((deallocate_tls_t)(cur_task->tls_ops.deallocate_tls))(meta->tcb_i, meta->dealloc_tcb_i);
    spin_unlock(&cur_task->tls_ops_lock);
    meta->ret_o = 0;
    //cluster_ctx_t* cur2 = ACCESS_ONCE(cur_cluster);
    //switch_to_stack(&cur2->api_user_stack);
    loadfs(cur_fs);
    return;
}

extern __thread int my_uipi_vector_cede_fd;
extern __thread int my_uipi_vector_yield_fd;

void vessel_register_uipi_handlers_api (void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    struct vessel_register_uipi_handlers_api_args* meta = args;
    cur->uipi_ops = *(meta->ops_i);
    meta->uipi_vector_cede_fd = my_uipi_vector_cede_fd;
    meta->uipi_vector_yield_fd = my_uipi_vector_yield_fd;
    meta->ret_o = 0;
    loadfs(cur_fs);
    return;
}

void vessel_get_cpu_info_api (void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    task_t *task = cur->mytask;
    size_t info_size = sizeof(cpu_info_t);
    cpu_info_t *cpu_info = ((malloc_func_t)cur->minimal_ops_je.malloc)(info_size);
    memcpy((void*)cpu_info, (void*) &(task->cpu_info), info_size);
    struct vessel_get_cpu_info_args* meta = args;
    meta->ret_o = cpu_info;
    loadfs(cur_fs);
    return;
}

/// @brief mwait.
/// @return void.
void vessel_cluster_mwait_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_mwait();
    loadfs(cur_fs);
    return;
}

/// @brief mwait.
/// @return int ret_o: 0 if success.
void vessel_cluster_id_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    struct vessel_cluster_id_args* meta = args;
    meta->ret_o = cluster_id();
    loadfs(cur_fs);
    return;
}

/// @brief to_steal.
/// @return int ret_o: 1 if this core needs to steal.
void vessel_cluster_to_steal_api(void* args) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    struct vessel_cluster_to_steal_args* meta = args;
    meta->ret_o = cluster_to_steal();
    loadfs(cur_fs);
    return;
}

int apis_init(void) {
    global_vessel_apis->alloc_chunk = vessel_alloc_chunk_api;
    global_vessel_apis->alloc_huge_chunk = vessel_alloc_huge_chunk_api;
    global_vessel_apis->cluster_spawn = vessel_cluster_spawn_api;
    global_vessel_apis->cluster_spawn_all = vessel_spawn_all_clusters_api;
    global_vessel_apis->cluster_yield = vessel_cluster_yield_api;
    global_vessel_apis->cluster_exit = vessel_cluster_exit_api;
    global_vessel_apis->create_tcb = vessel_create_tcb_api;
    global_vessel_apis->init_tcb = vessel_init_tcb_api;
    global_vessel_apis->dealloc_tcb = vessel_dealloc_tcb_api;
    global_vessel_apis->register_uipi_handlers = vessel_register_uipi_handlers_api;
    global_vessel_apis->get_cpu_info = vessel_get_cpu_info_api;
    global_vessel_apis->cluster_mwait = vessel_cluster_mwait_api;
    global_vessel_apis->cluster_id = vessel_cluster_id_api;
    global_vessel_apis->cluster_to_steal = vessel_cluster_to_steal_api;
    // TODO;
    return 0;
}