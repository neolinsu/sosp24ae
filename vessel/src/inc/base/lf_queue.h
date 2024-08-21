#pragma once
#include <base/stddef.h>
#include <base/atomic.h>
#include <base/log.h>

struct lf_quque_set_entry {
    atomic_t taken;
    char pad [CACHE_LINE_SIZE - 
                sizeof(int)];
} __aligned(CACHE_LINE_SIZE);

BUILD_ASSERT(sizeof(struct lf_quque_set_entry) == CACHE_LINE_SIZE);

struct lf_quque_set {
    struct lf_quque_set_entry meta[NCPU];
} __aligned(CACHE_LINE_SIZE);

BUILD_ASSERT(sizeof(struct lf_quque_set) % CACHE_LINE_SIZE == 0);


struct lf_queue {
    atomic_t* meta;
    size_t size;
    atomic_u32_t head;
    atomic_u32_t tail;
    char pad[CACHE_LINE_SIZE
        -sizeof(atomic_t*)
        -sizeof(size_t)
        -sizeof(atomic_u32_t)*2];
    //
    struct lf_quque_set set;
};

BUILD_ASSERT(offsetof(struct lf_queue, set) % CACHE_LINE_SIZE == 0);

#define LF_QUEUE_TAIL  (-1)
#define LF_QUEUE_HEAD  (-2)
#define LF_QUEUE_EMPTY (-3)

#if 0
static inline bool lf_queue_put(struct lf_queue *q, int new_val) {
    bool ret = false;
    while (true) {
        log_debug("while (true)");
        unsigned int ori_head = atomic_u32_read(&q->head), head;
        head     = ((ori_head + 1) % q->size);
        log_debug("head %u", head);
        ori_head = (ori_head % q->size);
        log_debug("ori_head %u", ori_head);

        int val = atomic_read(&q->meta[head]);
        log_debug("val %d", val);
        if (val == LF_QUEUE_TAIL) {
            ret = false;
            break;
        } else if (val != LF_QUEUE_EMPTY)
            continue;
        ret = atomic_cmpxchg(&q->meta[head], val, LF_QUEUE_HEAD);
        if (!ret) continue;
        atomic_write(&q->meta[ori_head], new_val);
        atomic_u32_fetch_and_add(&q->head, 1);
        ret = true;
        break;
    }
    return ret;
};
#endif

static inline void lf_queue_init(struct lf_queue *q, atomic_t *meta, size_t size) {
    q->meta = meta;
	q->size = size;
	atomic_u32_write(&q->head, 1);
	atomic_u32_write(&q->tail, 0);
	atomic_write(&q->meta[1], LF_QUEUE_HEAD);
	atomic_write(&q->meta[0], LF_QUEUE_TAIL);
    memset(&q->set, 0, sizeof(q->set));
};

static inline bool lf_queue_put_with_meta(struct lf_queue *q, atomic_t* meta, int new_val) {
    bool ret = false;
    ret  = atomic_cmpxchg(&((q->set.meta[new_val]).taken), 0, 1);
    if (ret == false) {
        return false;
    }
    ret = false;
    while (true) {
        unsigned int ori_head = atomic_u32_read(&q->head), head;
        head     = ((ori_head + 1) % q->size);
        ori_head = (ori_head % q->size);
        
        int val = atomic_read(&meta[head]);
        if (val == LF_QUEUE_TAIL) {
            ret = false;
            break;
        } else if (val != LF_QUEUE_EMPTY)
            continue;
        ret = atomic_cmpxchg(&meta[head], val, LF_QUEUE_HEAD);
        if (!ret) continue;
        atomic_write(&meta[ori_head], new_val);
        unsigned int updated = atomic_u32_fetch_and_add(&q->head, 1);
        log_debug("put one %d, updated: %d", new_val, updated);
        ret = true;
        break;
    }
    return ret;
};

static inline bool lf_queue_get(struct lf_queue *q, int *new_val) {
    bool ret = false;
    while (true) {
        unsigned int ori_tail = atomic_u32_read(&q->tail), tail;
        tail     = ((ori_tail + 1) % q->size);
        ori_tail = (ori_tail % q->size);
        int val = atomic_read(&q->meta[tail]);
        if (val == LF_QUEUE_HEAD) {
            ret = false;
            break;
        } else if (val < 0)
            continue;
        ret = atomic_cmpxchg(&q->meta[tail], val, LF_QUEUE_TAIL);
        if (!ret) continue;
        *new_val = val;
        atomic_write(&q->meta[ori_tail], LF_QUEUE_EMPTY);
        unsigned int updated = atomic_u32_fetch_and_add(&q->tail, 1);
        log_debug("get one %d, updated: %d", val, updated);
        ret = true;
        break;
    }
    if (ret) {
        ret  = atomic_cmpxchg(&((q->set.meta[*new_val]).taken), 1, 0);
        if (unlikely(ret == false)) {
            log_err("ret == false");
            abort();
        }
    }
    return ret;
};