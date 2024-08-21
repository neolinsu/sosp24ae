#include <sys/stat.h>

#include <errno.h>
#include <unistd.h>
#include <link.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/mman.h>

#include <asm/ops.h>

#include <base/log.h>
#include <base/env.h>
#include <base/lock.h>

#include <core/task.h>
#include <core/mem.h>
#include <core/cluster.h>
#include <core/kthread.h>
#include <core/startalloc.h>

#include <uexec/uexec.h>
#include <mpk/mpk.h>


char *task_startup_settings=NULL;
int task_startup_settings_num=0;

extern task_map_t *global_task_map;
extern minimal_ops_t *global_minimal_ops;
extern char **environ;

spinlock_t cluster_id2task_ptr_l;
void* cluster_id2task_ptr[NCLUSTER];

static inline int taskid2index (task_id_t tid) {
    return tid - 1;
}
static inline int index2taskid (int index) {
    return index + 1;
}


void* startup_aligned_alloc(size_t align, size_t n) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    void* res = ((aligned_alloc_func_t)cur->minimal_ops_je.aligned_alloc)(align, n);
    loadfs(cur_fs);
    return res;
}
void* startup_malloc(size_t n) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    void* res = ((malloc_func_t)cur->minimal_ops_je.malloc)(n);
    loadfs(cur_fs);
    return res;
}
void* startup_calloc(size_t n, size_t m) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    void* res = ((calloc_func_t)cur->minimal_ops_je.calloc)(n, m);
    loadfs(cur_fs);
    return res;
}
void* startup_realloc(void* ptr, size_t m) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    void* res = ((realloc_func_t)cur->minimal_ops_je.realloc)(ptr, m);
    loadfs(cur_fs);
    return res;
}
void startup_free(void* ptr) {
    uint64_t cur_fs = get_cur_fs();
    switch_to_kthread_fs();
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    ((free_func_t)cur->minimal_ops_je.free)(ptr);
    loadfs(cur_fs);
    return;
}
//TODO: close this handle in a proper way
static inline int minimal_jemalloc_init(void) {
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    task_t* cur_task = (task_t*)cur->mytask;

    cur_task->miniop_base = (uint64_t)startup_vessel_alloc_task_chunk(START_MEM_CHUNK_NUM);

    cur->minimal_ops_je.malloc = start_malloc;
    cur->minimal_ops_je.free = start_free;
    cur->minimal_ops_je.aligned_alloc = start_aligned_alloc;
    cur->minimal_ops_je.calloc = start_calloc;
    cur->minimal_ops_je.realloc = start_realloc;

    cur->minimal_ops.malloc         = startup_malloc;
    cur->minimal_ops.free           = startup_free;
    cur->minimal_ops.aligned_alloc  = startup_aligned_alloc;
    cur->minimal_ops.calloc         = startup_calloc;
    cur->minimal_ops.realloc        = startup_realloc;

    // TODO: Protect this jemalloc.
    return 0;
}

void _start_task(void* arg) {
    task_start_args_t *sarg = (task_start_args_t*) arg;
    struct stat statbuf;
    unsigned char *data, *src;
    int fd;
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster; 

    int ret = minimal_jemalloc_init();
    if (ret) {
        log_err("Fail to init jemalloc for %s\n",
                    strerror(ret));
    }

    fd = open(sarg->path, O_RDONLY);
    if(fstat(fd, &statbuf) == -1) {
		log_err("_start_task Failed to fstat(fd): %s\n", strerror(errno));
		cluster_exit();
	}

    data = (unsigned char*) ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(PAGE_SIZE, statbuf.st_size); // Alloc for current task.
    src = (unsigned char*) mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    memcpy((void*)data, (void*)src, statbuf.st_size);
	munmap(src, statbuf.st_size);
    close(fd);

    char** new_envs = gen_new_env(sarg->env, task_startup_settings,
                        task_startup_settings_num,
                        (malloc_func_t)(cur->minimal_ops.malloc),
                        (free_func_t)(cur->minimal_ops.free));
    char** new_argv = gen_new_argv(sarg->argc, sarg->argv,
                        (malloc_func_t)(cur->minimal_ops.malloc),
                        (free_func_t)(cur->minimal_ops.free));
    // TODO: STACK SIZE
	size_t *new_stack = (size_t *) (2046 * PAGE_SIZE +  (char *) ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(64, 2048 * PAGE_SIZE));
    reflect_execves(data, new_argv, new_envs, new_stack, sarg->uid);
    log_crit("Unknown branch.");
}

cluster_ctx_t* build_task_start_cluster(task_t *task, task_start_args_t* argv){
    cluster_ctx_t* new_cluster = alloc_cluster();
    set_software_entry(new_cluster, &_start_task, (void*) argv, NULL, NULL);
    new_cluster->mytask = task;
    atomic_write(&new_cluster->parked, false);
    new_cluster->to_start = true;
    new_cluster->with_fs = true;
    new_cluster->to_exit = false;
    new_cluster->miniop_l_ptr = &(task->miniop_l);
    new_cluster->miniop_cnt_ptr = &(task->miniop_cnt);
    new_cluster->miniop_base_ptr = &(task->miniop_base);
    new_cluster->miniop_last_ptr = &(task->miniop_last);
    bitmap_init(new_cluster->cpu_affinity, NCPU, false);
    bitmap_or(new_cluster->cpu_affinity, new_cluster->cpu_affinity, task->cpu_info.cpu_mask, NCPU);
    return new_cluster;
}

task_t* task_alloc() {
    int tmp_tid;
    task_t *new_task=NULL;
    spin_lock(&(global_task_map->lock));

    if (global_task_map->task_num == NTASK) {
        log_crit("Out of task num:%d.", NTASK);
        errno = EAGAIN;
        goto out;
    }
    tmp_tid = global_task_map->anchor;
    while(global_task_map->map[tmp_tid].active) {
        tmp_tid = (tmp_tid + 1)%NTASK;
    }
    global_task_map->anchor = tmp_tid + 1;
    global_task_map->task_num++;
    new_task = &(global_task_map->map[tmp_tid]);
    new_task->active = 1;
    new_task->task_id = index2taskid(tmp_tid);
    new_task->pid = getpid();
    new_task->mpk = taskid2mpk(new_task->task_id);
    spin_lock_init(&(new_task->mds_lock));
    list_head_init(&(new_task->mds));
    spin_lock_init(&(new_task->cluster_ctx_lock));
    list_head_init(&(new_task->cluster_ctx));
    spin_lock_init(&(new_task->tls_ops_lock));
    spin_lock_init(&(new_task->miniop_l));
    atomic64_write(&(new_task->miniop_cnt), 0l);
    new_task->miniop_base = 0;
out:
    spin_unlock(&(global_task_map->lock));
    return new_task;
}

int task_attach_cluster(task_t *task, cluster_ctx_t* cluster) {
    int ret = 0;

    ret = spin_try_lock(&task->cluster_ctx_lock);
    if (!ret) {
        return EAGAIN;
    }
    list_add_tail(&task->cluster_ctx, &cluster->task_link);
    spin_unlock(&task->cluster_ctx_lock);
    spin_lock(&cluster_id2task_ptr_l);
    cluster_id2task_ptr[cluster->id] = task;
    spin_unlock(&cluster_id2task_ptr_l);

    return 0;
}

int task_deattach_cluster(cluster_ctx_t* cluster) {
    int ret = 0;
    spin_lock(&cluster_id2task_ptr_l);
    task_t * task = cluster_id2task_ptr[cluster->id];
    spin_unlock(&cluster_id2task_ptr_l);
    
    ret = spin_try_lock(&task->cluster_ctx_lock);
    if (!ret) {
        return EAGAIN;
    }
    if (cluster->mytask == task) {
        list_del_from(&task->cluster_ctx, &cluster->task_link);
        ret = 0;
    } else {
        ret = EINVAL;
    }
    spin_unlock(&task->cluster_ctx_lock);
    return ret;
}

void* startup_vessel_alloc_task_chunk(size_t chunk_num) {
    cluster_ctx_t *cur = (cluster_ctx_t *)cur_cluster;
    task_t* cur_task = (task_t*)cur->mytask;
    log_info("startup_vessel_alloc_task_chunk");
    void* alloc_res = alloc_chunk(cur_task->task_id, chunk_num, &(cur_task->mds));
    return alloc_res;
}

void startup_vessel_register_tls_ops(struct tls_ops *ops) {
    cluster_ctx_t *cur = (cluster_ctx_t *)cur_cluster;
    task_t* cur_task = (task_t*)cur->mytask;
    cur_task->tls_ops.allocate_tls_storage = ops->allocate_tls_storage;
    cur_task->tls_ops.tls_init = ops->tls_init;
    cur_task->tls_ops.deallocate_tls = ops->deallocate_tls;
    return;
}

int task_meta_init(void) {
    memset(cluster_id2task_ptr, 0, sizeof(cluster_id2task_ptr));
    spin_lock_init(&cluster_id2task_ptr_l);
    return 0;
}