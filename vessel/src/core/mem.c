#include <sys/mman.h>
#include <linux/mman.h>
#include <linux/shm.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "base/compiler.h"
#include "base/log.h"
#include "base/list.h"
#include "base/mem.h"

#include "core/mem.h"
#include "core/config.h"

#include "mpk/mpk.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

void *__vessel_mem_start_ptr;
static int   __vessel_meta_fd;

static vessel_mem_meta_t * __vessel_meta_ptr = NULL;
static void * __vessel_data_ptr = NULL;
static void * __vessel_data_huge_ptr = NULL;

static inline int is_mem_inited() {
    return __vessel_mem_start_ptr == (void*)VESSEL_MEM_GLOBAL_START && 
			*((uint64_t*) __vessel_mem_start_ptr) == VESSEL_MAGIC_CODE;
}


static inline int md2index (int md) {
    return md - 1;
}
static inline int index2md (int index) {
    return index + 1;
}
static inline void* chunkid2ptr(uint64_t chunk_start) {
    return __vessel_data_ptr + chunk_start * VESSEL_MEM_BASIC_CHUNK_SIZE;
}
static inline void* hugechunkid2ptr(uint64_t chunk_start) {
    return __vessel_data_huge_ptr + chunk_start * VESSEL_MEM_BASIC_CHUNK_SIZE;
}

// Should hold the lock.
// Returns fd > 0. Return 0 for failure.
static inline int alloc_md(int tid, struct list_head * head, uint64_t chunk_num, uint64_t chunk_start) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int cnt=0, md;
    struct vessel_md_meta *i,
                *anchor_md = &(t->md_list[t->md_anchor]),
                *end_md    = &(t->md_list[NMEMD-1]),
                *start_md  = &(t->md_list[0]);
    for (i = anchor_md, cnt=0;
         cnt < NMEMD;
         cnt++, i=(i==end_md)?start_md:i+1)
    {
        if (i->tid == 0) {
            list_add(head, &(i->link));
            i->tid = tid;
            i->chunk_num = chunk_num;
            i->chunk_start = chunk_start;
            md = (t->md_anchor + cnt) % NMEMD;
            t->md_anchor = (md + 1) % NMEMD;
            for (int pos=chunk_start;
                    pos<chunk_start+chunk_num;
                    pos++) {
                bitmap_set(t->chunk_alloc, pos);
            }
            return index2md(md); // Can not return 0;
        }
    }
    errno = EAGAIN;
    log_crit("Fail to allocate memory descriptor.\n");
    return 0;
}

// Should hold the lock.
// Returns fd > 0. Return 0 for failure.
static inline int alloc_huge_md(int tid, struct list_head * head, uint64_t chunk_num, uint64_t chunk_start) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int cnt=0, md;
    struct vessel_md_meta *i,
                *anchor_md = &(t->huge_md_list[t->huge_md_anchor]),
                *end_md    = &(t->huge_md_list[NMEMD-1]),
                *start_md  = &(t->huge_md_list[0]);
    for (i = anchor_md, cnt=0;
         cnt < NMEMD;
         cnt++, i=(i==end_md)?start_md:i+1)
    {
        if (i->tid == 0) {
            list_add(head, &(i->link));
            i->tid = tid;
            i->chunk_num = chunk_num;
            i->chunk_start = chunk_start;
            md = (t->huge_md_anchor + cnt) % NMEMD;
            t->huge_md_anchor = (md + 1) % NMEMD;
            for (int pos=chunk_start;
                    pos<chunk_start+chunk_num;
                    pos++) {
                bitmap_set(t->huge_chunk_alloc, pos);
            }
            return index2md(md); // Can not return 0;
        }
    }
    errno = EAGAIN;
    log_crit("Fail to allocate huge memory descriptor.\n");
    return 0;
}

// Should hold the lock.
// 0 if success, otherwise failure.
static inline int dealloc_md(int tid, int md, struct list_head *head) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int index = md2index(md);
    if (unlikely(tid != t->md_list[index].tid)) {
        return -EACCES;
    }
    uint64_t chunk_num   = t->md_list[index].chunk_num;
    uint64_t chunk_start = t->md_list[index].chunk_start;
    list_del_from(head, &(t->md_list[index].link));
    t->md_list[index].tid = 0;
    for (int pos=chunk_start;
            pos<chunk_start+chunk_num;
            pos++) {
        bitmap_clear(t->chunk_alloc, pos);
    }
    return 0;
}

static inline void __init_mem_meta (void) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    log_info("do init mem meta");
    t->_magic = VESSEL_MAGIC_CODE;
    spin_lock_init(&(t->mem_lock));
    bitmap_init(t->chunk_alloc, VESSEL_CHUNK_NUM, false);
    bitmap_init(t->huge_chunk_alloc, VESSEL_HUGE_CHUNK_NUM, false);
    memset(t->md_list, 0, sizeof(t->md_list));
    memset(t->huge_md_list, 0, sizeof(t->huge_md_list));
    t->md_anchor = 0;
    t->huge_md_anchor = 0;
}

/**
 * init_mem - ...
 * Returns 0 successful, otherwise failure.
 */
int mem_init(int force) {
    int ret;
    char buf[100];
    sprintf(buf, "vessel_meta_%s", vessel_name);
	__vessel_meta_fd = shm_open(buf, O_CREAT | O_RDWR, VESSEL_META_PRIO);
    if (!__vessel_meta_fd) {
        log_emerg("Fail to open shared meta memory (key: %s), for %s.",
                    VESSEL_DATA_KEY,
                    strerror(errno));
        return errno;
    }
    ret = ftruncate(__vessel_meta_fd, VESSEL_MEM_META_SIZE);
    if (ret) {
        log_emerg("Fail to fturncate meta memory fd (%d), for %s.",
                    __vessel_meta_fd,
                    strerror(errno));
        return errno;
    }

    __vessel_meta_ptr = (vessel_mem_meta_t*) mmap(
        (void*)VESSEL_MEM_GLOBAL_START, VESSEL_MEM_META_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, __vessel_meta_fd, 0);
    __vessel_mem_start_ptr  = (void*) __vessel_meta_ptr;
    if (MAP_FAILED == __vessel_mem_start_ptr || (void*) VESSEL_MEM_GLOBAL_START != __vessel_mem_start_ptr) {
        log_emerg("Fail to mmap meta memory for %s.",
                    strerror(errno));
        return errno;
    }

    if (is_mem_inited()) {
        log_info("mem has been inited.");
    }

    if (force || !is_mem_inited()) {
        log_info("Force mem init.");
        __init_mem_meta();
    }

    __vessel_data_ptr = mem_map_shm(hash_name(vessel_name,"vessel_data!#"), (void*) VESSEL_MEM_DATA_START, VESSEL_MEM_DATA_SIZE, PGSIZE_4KB, false);
    if (MAP_FAILED == __vessel_data_ptr || __vessel_data_ptr != (void*) VESSEL_MEM_DATA_START) {
        log_emerg("Fail to mmap data memory for vessel_data for %s.",
                    strerror(errno));
        return errno;
    }

    __vessel_data_huge_ptr = mem_map_shm(hash_name(vessel_name,"vessel_data_huge!#"), (void*) VESSEL_MEM_DATA_HUGE_START, VESSEL_MEM_DATA_HUGE_SIZE, PGSIZE_2MB, false);
    if (MAP_FAILED == __vessel_data_huge_ptr || __vessel_data_huge_ptr != (void*) VESSEL_MEM_DATA_HUGE_START) {
        log_emerg("Fail to mmap data huge memory for vessel_data for %s.",
                    strerror(errno));
        return errno;
    }
    log_debug("the huge is at %p to %p\n", __vessel_data_huge_ptr, __vessel_data_huge_ptr + VESSEL_MEM_DATA_HUGE_SIZE);
    return 0;
};

/**
 * exit_mem - ...
 * Returns 0 successful, otherwise failure.
 */
int exit_mem(int tid, struct list_head *head) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int ret;
    spin_lock(&(t->mem_lock));
    for (int i=0; i<NMEMD; i++) {
        if (t->md_list[i].tid == tid) {
            ret = dealloc_md(tid, index2md(i), head);
            if (ret) {
                log_crit("md crash %d.", i);
                goto fail;
            }
        }
    }
    goto ok;
fail:
    log_crit("Fail to free memory chunk.\n");
ok:
    spin_unlock(&(t->mem_lock));
    return 0;
}

/**
 * clear_mem - ...
 * Returns 0 successful, otherwise failure.
 */
int clear_mem() {
    int ret;
    
    ret = munmap(__vessel_meta_ptr, VESSEL_MEM_META_SIZE);
    if (ret) {
        log_emerg("Fail to munmap meta memory (%p), for %s.",
                    __vessel_meta_ptr,
                    strerror(errno));
        return errno;
    }
    char buf[100];
    sprintf(buf, "vessel_mem_%s", vessel_name);
    ret = shm_unlink(buf);
    if (ret) {
        log_emerg("Fail to shm_unlink meta memory fd (%d), for %s.",
                    __vessel_meta_fd,
                    strerror(errno));
        return errno;
    }

    // TODO
    return 0;
};

/**
 * alloc_chunk - ...
 * Returns fd > 0. Return 0 for failure.
 */
void* alloc_chunk(int tid, size_t chunk_num, struct list_head *head) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int index, preindex, ret=0;
    void* res = NULL;
    size_t ava_chunk = 0;
    spin_lock(&(t->mem_lock));
    // log_info("[alloc_chunk]: %lu", chunk_num);

    if (VESSEL_CHUNK_NUM - bitmap_popcount(t->chunk_alloc, VESSEL_CHUNK_NUM) < chunk_num) {
        errno = ENOMEM;
        goto fail;
    }
    preindex = -1;
    bitmap_for_each_cleared(t->chunk_alloc, VESSEL_CHUNK_NUM, index) {
        if (preindex!=index-1 || preindex==-1) {
            ava_chunk = 0;
        }
        preindex = index;
        if (++ava_chunk == chunk_num) {
            ret = alloc_md(tid, head, chunk_num, index-chunk_num+1);
            if (unlikely(!ret)) goto fail;
            res = chunkid2ptr((uint64_t) index - chunk_num + 1);
            ret = pkey_mprotect(res, VESSEL_MEM_BASIC_CHUNK_SIZE * chunk_num, PROT_READ | PROT_WRITE , taskid2mpk(tid));
            if (unlikely(ret)) goto fail;
            goto ok;
        }
    }
fail:
    log_crit("Fail to allocate memory chunk, for %s", strerror(errno));
ok:
    spin_unlock(&(t->mem_lock));
    //log_info("BACK");
    return res;
};

/**
 * alloc_huge_chunk - ...
 * Returns fd > 0. Return 0 for failure.
 */
void* alloc_huge_chunk(int tid, size_t chunk_num, struct list_head *head) {
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    int index, preindex, ret=0;
    void* res = NULL;
    size_t ava_chunk = 0;
    spin_lock(&(t->mem_lock));
    if (chunk_num == 0) {
        errno = EINVAL;
        goto fail;
    }
    if (VESSEL_HUGE_CHUNK_NUM - bitmap_popcount(t->huge_chunk_alloc, VESSEL_HUGE_CHUNK_NUM) < chunk_num) {
        errno = ENOMEM;
        goto fail;
    }
    preindex = -1;
    bitmap_for_each_cleared(t->huge_chunk_alloc, VESSEL_HUGE_CHUNK_NUM, index) {
        if (preindex!=index-1 || preindex==-1) {
            ava_chunk = 0;
        }
        preindex = index;
        if (++ava_chunk == chunk_num) {
            ret = alloc_huge_md(tid, head, chunk_num, index-chunk_num+1);
            if (unlikely(!ret)) goto fail;
            res = hugechunkid2ptr((uint64_t) index - chunk_num + 1);
            log_info("alloc huge chunk: %p to %p for chunk_num: %lu", res, res + VESSEL_MEM_BASIC_CHUNK_SIZE * chunk_num, chunk_num);
            ret = pkey_mprotect(res, VESSEL_MEM_BASIC_CHUNK_SIZE * chunk_num, PROT_READ | PROT_WRITE , taskid2mpk(tid));
            if (unlikely(ret)) {
                log_err("Fail to pkey_mprotect for %s, @%p", strerror(errno), res);
                goto fail;
            }
            goto ok;
        }
    }
ok:
    spin_unlock(&(t->mem_lock));
    return res;
fail:
    while(1);
    spin_unlock(&(t->mem_lock));
    log_crit("Fail to allocate memory chunk, for %s", strerror(errno));
    return NULL;
};

/**
 * free_chunk - ...
 * Returns 0 successful, otherwise failure.
 */
int free_chunk(int tid, int md, struct list_head *head) {
    int ret;
    vessel_mem_meta_t *t = __vessel_meta_ptr;
    spin_lock(&(t->mem_lock));
    ret = dealloc_md(tid, md, head);
    spin_unlock(&(t->mem_lock));
    return ret;
}

