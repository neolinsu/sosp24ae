#include <stddef.h>
#include <stdint.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/lock.h>

#include <core/cluster.h>
#include <core/kthread.h>


#define START_MEM_END 1llu*GB
void *
start_malloc (size_t n)
{
    cluster_ctx_t * cur = cur_cluster;
    spin_lock(cur->miniop_l_ptr);
    barrier();
    /* Make sure the allocation pointer is ideally aligned.  */
    
    uint64_t alup = align_up((uint64_t)(atomic64_read(cur->miniop_cnt_ptr)), 8);
    *(cur->miniop_last_ptr) = alup;
    if(alup > START_MEM_END || alup + n > START_MEM_END) {
        log_debug("alup>START_MEM_END");
        abort();
    }
    uint64_t next = alup + n;
    atomic64_write(cur->miniop_cnt_ptr, next);
    barrier();
    spin_unlock(cur->miniop_l_ptr);
    return (void*) *(cur->miniop_base_ptr) + alup;
}


void *
start_aligned_alloc (size_t align, size_t len)
{
    cluster_ctx_t * cur = cur_cluster;
    spin_lock(cur->miniop_l_ptr);
    barrier();
    /* Make sure the allocation pointer is ideally aligned.  */

    uint64_t alup = align_up((uint64_t)(atomic64_read(cur->miniop_cnt_ptr)), align);
    len = align_up(len, align);
    if(alup > START_MEM_END || alup + len > START_MEM_END) {
        log_debug("alup>START_MEM_END");
        abort();
    }
    uint64_t next = alup + len;
    atomic64_write(cur->miniop_cnt_ptr, next);
    barrier();
    spin_unlock(cur->miniop_l_ptr);
    return (void*)*(cur->miniop_base_ptr) + alup;
}

void *
start_calloc (size_t num, size_t size)
{
    cluster_ctx_t * cur = cur_cluster;
    size_t n = num * size;
    spin_lock(cur->miniop_l_ptr);
    barrier();
    /* Make sure the allocation pointer is ideally aligned.  */
    
    uint64_t alup = align_up((uint64_t)(atomic64_read(cur->miniop_cnt_ptr)), CACHE_LINE_SIZE);
    if(alup>START_MEM_END) {
        log_debug("alup>START_MEM_END");
        abort();
    }
    uint64_t next = alup + n;
    atomic64_write(cur->miniop_cnt_ptr, next);
    barrier();
    spin_unlock(cur->miniop_l_ptr);
    void* res = (void*)*(cur->miniop_base_ptr) + alup;
    memset(res, 0, n);
    return res;
}

void *
start_realloc (void *ptr, size_t size)
{
    cluster_ctx_t * cur = cur_cluster;
    if (ptr == NULL)
        return start_malloc (size);
    
    assert (ptr == (void*)*(cur->miniop_last_ptr));
    size_t old_size = atomic64_read(cur->miniop_cnt_ptr) - *(cur->miniop_last_ptr);
    atomic64_write(cur->miniop_cnt_ptr, *(cur->miniop_last_ptr));
    void *new = start_malloc (size);
    return new != ptr ? memcpy (new, ptr, old_size) : new;
}

/* This will rarely be called.  */
void
start_free (void *ptr)
{
  log_debug("Try to free");
}
