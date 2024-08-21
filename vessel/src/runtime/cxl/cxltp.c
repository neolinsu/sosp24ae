#include <sys/mman.h>

#include <numaif.h>

#include <base/stddef.h>
#include <base/mem.h>

#include <runtime/timer.h>
#include <runtime/minialloc.h>
#include <runtime/cxl/cxltp.h>
#include <runtime/tls/thread.h>
#include <runtime/tls/tcache.h>
#include <runtime/mmem/mempool.h>

#include "./waitq.h"
#include "./tqueue.h"
#include "cxltp_defs.h"

#include <numa.h>

cxltp_queue_pool_meta_t *global_cxltp_queue_pool_meta;
cxltp_ring_pool_meta_t *global_cxltp_ring_pool;

bool cfg_cxltp_enabled = false;

#ifdef VESSEL_CXLTP

bool force_init_cxltp_mem = 0;

struct mempool cxltp_tx_ring_buf_mp;
static struct tcache *cxltp_tx_ring_buf_tcache;
static DEFINE_PERTHREAD(struct tcache_perthread, cxltp_tx_ring_buf_pt);

struct mempool cxltp_tx_buf_mp;
static struct tcache *cxltp_tx_buf_tcache;
static DEFINE_PERTHREAD(struct tcache_perthread, cxltp_tx_buf_pt);

DEFINE_PERTHREAD(tqueue_t, perk_tq);

int cxltp_ring_alloc_from_pool(
    cxltp_ring_pool_meta_t *pool,
    void *start,
    void *end,
    atomic64_t **head,
    atomic64_t **tail)
{
    int ret;
    spin_lock_np(&pool->alloced_lock);
    int index;
    bitmap_for_each_cleared(pool->alloced, MSG_RING_POOL_LENGTH, index)
    {
        *head = &(pool->heads[index]);
        *tail = &(pool->tails[index]);
        pool->starts[index] = start;
        pool->ends[index] = end;
        bitmap_atomic_set(pool->alloced, index);
        ret = 0;
        goto out;
    }
    *head = NULL;
    *tail = NULL;
    ret = -EAGAIN;
out:
    spin_unlock_np(&pool->alloced_lock);
    return ret;
}

int cxltp_ring_put_to_pool(cxltp_ring_pool_meta_t *pool, atomic64_t *head)
{
    int ret;
    int index = head - pool->heads;
    spin_lock_np(&pool->alloced_lock);
    if (!bitmap_atomic_test(pool->alloced, index))
    {
        log_err("Ring to put is not allocated.");
        ret = -EINVAL;
        goto out;
    }
    bitmap_atomic_clear(pool->alloced, index);
    ret = 0;
out:
    spin_unlock_np(&pool->alloced_lock);
    return ret;
}

int cxltp_tx_ring_alloc(cxltp_ring_pool_meta_t *pool, cxltp_ring_meta_t *out)
{
    int ret;
    cxltp_ring_data_entry_t *d = tcache_alloc(&perthread_get(cxltp_tx_ring_buf_pt));
    if (d == NULL)
    {
        log_err("d==NULL");
        return -ENOMEM;
    }
    memset(d, 0, sizeof(*d));
    out->start = (void *)&(d->payload[0]);
    out->end = (void *)&(d->payload[MSG_RING_LENGTH]);
    // log_info("cxltp_tx_ring_alloc ONE -> start: %p, end: %p", out->start, out->end);
    out->timeout = 0;
    out->pool = pool;
    ret = cxltp_ring_alloc_from_pool(pool, out->start, out->end, &out->head_ptr, &out->tail_ptr);
    if (ret)
    {
        log_err("Fail to alloc ring from pool @%p.", pool);
        tcache_free(&perthread_get(cxltp_tx_ring_buf_pt), d);
        return ret;
    }
    atomic64_write(out->head_ptr, (uint64_t)(out->start));
    atomic64_write(out->tail_ptr, (uint64_t)(out->start));
    // log_info("cxltp_tx_ring_alloc ONE -> head_ptr: %p, tail_ptr: %p",
    //     atomic64_read(out->head_ptr),
    //     atomic64_read(out->tail_ptr));
    return 0;
}

void cxltp_ring_release(cxltp_ring_meta_t *out)
{
    int ret;
    ret = cxltp_ring_put_to_pool((cxltp_ring_pool_meta_t *)out->pool, out->head_ptr);
    if (ret)
    {
        log_err("Fail to cxltp_ring_put_to_pool for %p", out);
    }
    preempt_disable();
    for (void **i = (void **)out->start; i <= (void **)out->end; ++i)
    {
        if (*i != NULL)
        {
            tcache_free(&perthread_get(cxltp_tx_buf_pt), *i);
        }
    }
    tcache_free(&perthread_get(cxltp_tx_ring_buf_pt), out->start);
    preempt_enable();
}

cxltpconn_t *cxltp_conn_alloc(cxltp_ring_pool_meta_t *pool)
{
    int ret;
    cxltpconn_t *c;
    c = smalloc(sizeof(*c));
    if (c == NULL)
    {
        return NULL;
    }
    ret = cxltp_tx_ring_alloc(pool, &c->tx_ring);
    if (ret)
    {
        log_err("Fail to alloc tx ring for cxltp.");
        sfree(c);
        return NULL;
    }
    spin_lock_init(&c->l);
    c->shutdown = false;
    c->start = 0;
    c->sum = 0;
    c->cnt = 0;
    kref_init(&c->ref);
    return c;
};

static void cxltp_conn_release(struct kref *r)
{
    cxltpconn_t *c = container_of(r, cxltpconn_t, ref);
    cxltp_ring_release(&c->tx_ring);
    sfree(c);
}

void cxltp_conn_destroy(cxltpconn_t *c)
{
    // TODO: Seems nothing to do here.
    kref_put(&c->ref, cxltp_conn_release);
};

static void listen_worker(void *args)
{
    cxltpqueue_t *q = (cxltpqueue_t *)args;
    int ret = 0, retry, succeed;
    thread_t *th;
    cxltpconn_t *c;
    spin_lock_np(&q->cl);
    if (!q->shutdown)
    {
        kref_get(&q->ref);
    }
    else
    {
        log_debug("listener q shutdown unexpectedly");
        return;
    }
    spin_unlock_np(&q->cl);
    cxltp_queue_pool_meta_t *queue_pool = q->pool;
    while (true)
    {
        if (!bitmap_atomic_test(queue_pool->alloced, q - queue_pool->queues))
        {
            log_err("Port: %ld, CXLTP meta is broken.", q - queue_pool->queues);
            ret = -EPIPE;
            return;
        }
        spin_lock_np(&q->cl);
        if (q->backlog == 0)
        {
            ret = -1;
            log_debug("shutdown.");
            goto unlock;
        }
        if (q->shutdown)
        {
            ret = -1;
            log_debug("shutdown.");
            goto unlock;
        }
        BUG_ON(atomic64_read(&q->server_state) != server_state_idle);
        if (atomic64_read(&q->client_state) == client_state_ready)
        {
            c = cxltp_conn_alloc(q->ring_pool);
            if (!c)
            {
                log_err("NOMEM in listener.");
                goto unlock;
            }
            c->rx_ring = q->client_ring_meta;
            ACCESS_ONCE(q->server_ring_meta) = c->tx_ring;
            atomic64_write(&q->server_state, server_state_ready);
        }
        else
        {
            goto unlock;
        }
        retry = 0, succeed = 0;
        while (retry < MSG_DAIL_RETRY)
        {
            spin_unlock_np(&q->cl);
            timer_sleep(MSG_LISTEM_INTERV * ONE_MS);
            spin_lock_np(&q->cl);

            if (atomic64_read(&q->client_state) == client_state_done)
            {
                log_debug("client_state_done");
                succeed = true;
                break;
            }
            else if (atomic64_read(&q->client_state) == client_state_ready)
            {
                retry++;
                continue;
            }
            else
            {
                atomic64_write(&q->server_state, server_state_idle);
                goto unlock;
            }
        }
        if (succeed)
        {
            log_debug("Add one ccon");
            spin_lock_np(&q->l);
            list_add_tail(&q->conns, &c->queue_link);
            th = waitq_signal(&q->wq, &q->l);
            spin_unlock_np(&q->l);
            waitq_signal_finish(th);
            atomic64_write(&q->server_state, server_state_idle);
            q->backlog--;
        }
        else
        {
            log_err("Time out for waiting client.");
            atomic64_write(&q->server_state, server_state_idle);
        }
    unlock:
        spin_unlock_np(&q->cl);
        if (ret)
        {
            return;
        }
        else
        {
            timer_sleep(MSG_LISTEM_INTERV * ONE_MS);
        }
    }
}

cxltpqueue_t *cxltp_queue_alloc(void)
{
    if (!CXLTP_IS_POOL(global_cxltp_queue_pool_meta))
    {
        log_err("global_cxltp_queue_pool_meta is wrong");
        return NULL;
    }
    int index;
    cxltp_queue_pool_meta_t *pool = global_cxltp_queue_pool_meta;
    spin_lock_np(&pool->alloced_lock);
    bitmap_for_each_cleared(pool->alloced, MSG_QUEUE_POOL_LENGTH, index)
    {
        bitmap_atomic_set(pool->alloced, index);
        pool->queues[index].pool = global_cxltp_queue_pool_meta; // todo
        spin_unlock_np(&pool->alloced_lock);
        return (pool->queues) + index;
    }
    spin_unlock_np(&pool->alloced_lock);
    return NULL;
}

void cxltp_queue_release(struct kref *ref)
{
    cxltpqueue_t *q = container_of(ref, cxltpqueue_t, ref);
    if (!CXLTP_IS_POOL((cxltp_queue_pool_meta_t *)q->pool))
    {
        log_err("global_cxltp_queue_pool_meta is wrong");
        return;
    }
    cxltp_queue_pool_meta_t *pool = q->pool;
    spin_lock_np(&pool->alloced_lock);
    if (bitmap_atomic_test(pool->alloced, q - pool->queues))
    {
        spin_unlock_np(&pool->alloced_lock);
        bitmap_atomic_clear(pool->alloced, q - pool->queues);
    }
    else
    {
        spin_unlock_np(&pool->alloced_lock);
        log_err("Double free");
    }

    return;
}

void cxltp_queue_destroy(cxltpqueue_t *q)
{
    // TODO: Seems nothing to do here.
    kref_put(&q->ref, cxltp_queue_release);
};

static void __cxltp_qshutdown(cxltpqueue_t *q)
{
    /* mark the listen queue as shutdown */
    spin_lock_np(&q->l);
    BUG_ON(q->shutdown);
    q->shutdown = true;
    spin_unlock_np(&q->l);
}

void cxltp_qshutdown(cxltpqueue_t *q)
{
    /* mark the listen queue as shutdown */
    __cxltp_qshutdown(q);
}

void cxltp_qclose(cxltpqueue_t *q)
{
    cxltpconn_t *c, *nextc;

    if (!q->shutdown)
        __cxltp_qshutdown(q);

    BUG_ON(!waitq_empty(&q->wq));

    /* free all pending connections */
    list_for_each_safe(&q->conns, c, nextc, queue_link)
    {
        list_del_from(&q->conns, &c->queue_link);
        cxltp_conn_destroy(c);
    }

    cxltp_queue_destroy(q);
}

int cxltp_listen(cxltp_addr_t *cxltp_addr_arg, size_t backlog, cxltpqueue_t **q_out)
{
    cxltp_addr_t *cxltp_addr = cxltp_addr_arg;
    cxltpqueue_t *q;
    int ret;
    if (!cxltp_addr || cxltp_addr->base != NULL || cxltp_addr->port != 0)
    {
        return -EINVAL;
    }
    q = cxltp_queue_alloc();
    if (!q)
    {
        return -ENOMEM;
    }

    spin_lock_init(&q->l);
    list_head_init(&q->conns);
    waitq_init(&q->wq);
    q->shutdown = false;
    q->backlog = backlog;
    spin_lock_init(&q->cl);
    atomic64_write(&q->client_state, client_state_idle);
    atomic64_write(&q->server_state, server_state_idle);
    kref_init(&q->ref);
    q->ring_pool = global_cxltp_ring_pool; // TODO
    q->status.heads = q->ring_pool->heads;
    q->status.tails = q->ring_pool->tails;
    q->status.size = MSG_RING_POOL_LENGTH;
    q->status.shutdown_ptr = &q->shutdown;
    cxltp_addr->base = q->pool;
    cxltp_addr->port = q - ((cxltp_queue_pool_meta_t *)q->pool)->queues;
    log_info("[CXLTP] Listen at %p:%lu", cxltp_addr->base, cxltp_addr->port);
    // todo link pool;
    ret = thread_spawn(listen_worker, q);
    if (ret)
    {
        log_err("Fail to thread_spawn listen_worker.");
        cxltp_queue_release(&q->ref);
        return ret;
    }
    log_debug("Listen worker spawned. qout:%p", q_out);
    *q_out = q;
    return ret;
}

int cxltp_dial(cxltp_addr_t cxltp_addr, cxltpconn_t **c_out)
{
    if (!cfg_cxltp_enabled)
    {
        log_err("cxltp is not enabled.");
        return -EACCES;
    }
    log_debug("into cxltp_dial, cxltp_addr.base=%p, cxltp_addr.port=%lu", cxltp_addr.base, cxltp_addr.port);
    if (cxltp_addr.base == NULL)
    {
        log_err("cxltp_addr.base %lx is NULL", cxltp_addr.port);
        return -EACCES;
    }
    cxltp_queue_pool_meta_t *queue_pool = cxltp_addr.base;
    if (queue_pool->magic != CXLTP_MAGIC)
    {
        log_err("Fail to reg cxltp_queue_pool");
        return -EACCES;
    }
    if (!bitmap_atomic_test(queue_pool->alloced, cxltp_addr.port))
    {
        log_err("Fail to reg cxltp_queue_pool for unallocated port %lu", cxltp_addr.port);
        return -EACCES;
    }
    cxltpqueue_t *queue = &(queue_pool->queues[cxltp_addr.port]);
    cxltpconn_t *c = cxltp_conn_alloc(global_cxltp_ring_pool);
    int retry = 0, succeed;
    retry = 0;

    while (!atomic64_cmpxchg(&queue->client_state, client_state_idle, client_state_taken))
    {
        timer_sleep(MSG_LISTEM_INTERV * ONE_MS);
        if (retry++ > MSG_DAIL_RETRY)
        {
            log_info("Get queue->client_state timeout");
            return -ETIMEDOUT;
        }
    }
    ACCESS_ONCE(queue->client_ring_meta) = c->tx_ring;
    barrier();
    atomic64_write(&queue->client_state, client_state_ready);
    retry = 0, succeed = 0;
    while (retry++ < MSG_DAIL_RETRY)
    {
        if (atomic64_read(&queue->server_state) == server_state_ready)
        {
            succeed = 1;
            break;
        }
        timer_sleep(MSG_LISTEM_INTERV * ONE_MS);
    }
    if (!succeed)
    {
        cxltp_conn_destroy(c);
        atomic64_write(&queue->client_state, client_state_idle);
        log_debug("ready timeout");
        return -ETIMEDOUT;
    }
    c->rx_ring = ACCESS_ONCE(queue->server_ring_meta);
    barrier();
    atomic64_write(&queue->client_state, client_state_done);
    retry = 0, succeed = 0;
    while (retry++ < MSG_DAIL_RETRY)
    {
        if (atomic64_read(&queue->server_state) == server_state_idle)
        {
            succeed = 1;
            break;
        }
        timer_sleep(MSG_LISTEM_INTERV * ONE_MS);
    }
    if (!succeed)
    {
        cxltp_conn_destroy(c);
        atomic64_write(&queue->client_state, client_state_idle);
        log_debug("done timeout");
        return -ETIMEDOUT;
    }
    atomic64_write(&queue->client_state, client_state_idle);
    *c_out = c;
    return 0;
}

int cxltp_accept(cxltpqueue_t *q, cxltpconn_t **c_out)
{
    cxltpconn_t *c;

    spin_lock_np(&q->l);
    while (list_empty(&q->conns) && !q->shutdown)
        waitq_wait(&q->wq, &q->l);

    /* was the queue drained and shutdown? */
    if (list_empty(&q->conns) && q->shutdown)
    {
        spin_unlock_np(&q->l);
        return -EPIPE;
    }
    kref_get(&q->ref); // TODO:
    c = list_pop(&q->conns, cxltpconn_t, queue_link);
    assert(c != NULL);
    spin_unlock_np(&q->l);

    *c_out = c;
    return 0;
}

static inline int try_read_payload(
    const void *start, const void *end,
    atomic64_t *head, atomic64_t *tail, void **payload)
{
    if (atomic64_read(head) != atomic64_read(tail))
    {
        *payload = *((void **)atomic64_read(tail));
        return 0;
    }
    return -EAGAIN;
}

static inline void do_pop_payload(
    const void *start, const void *end,
    atomic64_t *head, atomic64_t *tail)
{
    atomic64_write(tail, atomic64_read(tail) == (uint64_t)end - 8 ? (uint64_t)start : atomic64_read(tail) + 8);
}

static inline ssize_t cxltp_read_wait(cxltpconn_t *c, size_t len, char *buf, bool force)
{
    struct cxltp_package_headr *m;
    char *pos = buf;
    ssize_t readlen = 0;
    spin_lock_np(&c->l);
    while (unlikely(
        try_read_payload(
            c->rx_ring.start, c->rx_ring.end,
            c->rx_ring.head_ptr, c->rx_ring.tail_ptr,
            (void **)&m)))
    {
        if (unlikely(force))
            return -EAGAIN;
        tq_wait(&myk()->cxltp_rx.tq, c);
        if (unlikely(c->shutdown))
        {
            spin_unlock_np(&c->l);
            return -ECONNABORTED;
        }
    }
    if (c->shutdown)
    {
        spin_unlock_np(&c->l);
        return -ECONNABORTED;
    }

    if (unlikely(m->type == error_package))
    {
        c->shutdown = true;
        do_pop_payload(
            c->rx_ring.start, c->rx_ring.end,
            c->rx_ring.head_ptr, c->rx_ring.tail_ptr);
        spin_unlock_np(&c->l);
        return -ECONNABORTED;
    }
    if (m->size > len - readlen)
    {
        memcpy(pos, m + sizeof(*m) + m->head, len - readlen);
        m->size -= len - readlen;
        m->head += len - readlen;
        pos += len - readlen;
        readlen = len;
        barrier();
    }
    else
    {
        memcpy(pos, m + sizeof(*m) + m->head, m->size);
        readlen += m->size;
        pos += m->size;
        m->type = to_free_package;
        do_pop_payload(
            c->rx_ring.start, c->rx_ring.end,
            c->rx_ring.head_ptr, c->rx_ring.tail_ptr);
    }
    spin_unlock_np(&c->l);
    return readlen;
}

static inline void cxltp_read_finish(cxltpconn_t *c, ssize_t readlen)
{
    if (readlen <= 0)
        return;
}

ssize_t cxltp_read(cxltpconn_t *c, void *buf, size_t len)
{
    ssize_t readlen = cxltp_read_wait(c, len, buf, false);

    cxltp_read_finish(c, readlen);
    return readlen;
}

static inline ssize_t cxltp_do_read(cxltpconn_t *c, struct cxltp_package_headr *m, size_t len, char *buf)
{
    char *pos = buf;
    ssize_t readlen = 0;

    if (c->shutdown)
    {
        return -ECONNABORTED;
    }
    if (unlikely(m->type == error_package))
    {
        c->shutdown = true;
        do_pop_payload(
            c->rx_ring.start, c->rx_ring.end,
            c->rx_ring.head_ptr, c->rx_ring.tail_ptr);
        return -EAGAIN;
    }
    if (m->size > len - readlen)
    {
        memcpy(pos, m + sizeof(*m) + m->head, len - readlen);
        m->size -= len - readlen;
        m->head += len - readlen;
        pos += len - readlen;
        readlen = len;
        barrier();
    }
    else
    {
        memcpy(pos, m + sizeof(*m) + m->head, m->size);
        readlen += m->size;
        pos += m->size;
        m->type = to_free_package;
        do_pop_payload(
            c->rx_ring.start, c->rx_ring.end,
            c->rx_ring.head_ptr, c->rx_ring.tail_ptr);
    }
    return readlen;
}

ssize_t cxltp_try_read(cxltpconn_t *c, void *buf, size_t len)
{
    ssize_t ret;
    struct cxltp_package_headr *m;
    spin_lock_np(&c->l);
    ret = try_read_payload(
        c->rx_ring.start, c->rx_ring.end,
        c->rx_ring.head_ptr, c->rx_ring.tail_ptr,
        (void **)&m);
    if (ret)
    {
        spin_unlock_np(&c->l);
        return ret;
    }
    ssize_t readlen = cxltp_do_read(c, m, len, buf);
    spin_unlock_np(&c->l);
    return readlen;
}

static inline int can_write_payload(
    const void *start, const void *end,
    atomic64_t *head, atomic64_t *tail)
{
    uint64_t new_head = atomic64_read(head);
    new_head = new_head == (uint64_t)end - 8 ? (uint64_t)start : new_head + 8;
    uint64_t bound = atomic64_read(tail);
    if (new_head != bound)
    {
        return 0;
    }
    return -EAGAIN;
}

static inline int do_write_payload(
    const void *start, const void *end,
    atomic64_t *head, atomic64_t *tail, void *payload)
{
    uint64_t new_head = atomic64_read(head);
    new_head = new_head == (uint64_t)end - 8 ? (uint64_t)start : new_head + 8;

    uint64_t bound = atomic64_read(tail);
    if (new_head != bound)
    {
        (*(void **)atomic64_read(head)) = payload;
        barrier();
        atomic64_write(head, new_head);
    }
    else
    {
        return -EAGAIN;
    }

    return 0;
}

static inline ssize_t cxltp_write_wait(cxltpconn_t *c, size_t len, const char *buf)
{
    const char *pos = buf;
    ssize_t writelen = 0;
    bool new_m = false;
    while (writelen < len)
    {
        void **cur_head = (void **)atomic64_read(c->tx_ring.head_ptr);
        struct cxltp_package_headr *m = NULL;
        if (*cur_head != NULL)
        {
            enum package_type type = ((struct cxltp_package_headr *)*cur_head)->type;
            if (unlikely(type == error_package))
            {
                return -103;
            }
            new_m = false;
            m = *cur_head;
        }
        else
        {
            new_m = true;
            m = tcache_alloc(&perthread_get(cxltp_tx_buf_pt));
        }
        if (!m)
        {
            return writelen;
        }
        m->head = 0;
        m->size = MSG_BUF_SIZE - sizeof(*m);
        char *m_base = (char *)(m + sizeof(*m));
        spin_lock_np(&c->l);

        if (unlikely(c->shutdown))
        {
            spin_unlock_np(&c->l);
            if (new_m)
                tcache_free(&perthread_get(cxltp_tx_buf_pt), m);
            return -103;
        }
        size_t clen = 0;
        clen = m->size > len - writelen ? len - writelen : m->size;
        memcpy(m_base, pos, clen);
        m->type = data_package;
        m->size = clen;
        spin_unlock_np(&c->l);
        /* block until there is an actionable event */
        while (unlikely(
            do_write_payload(
                c->tx_ring.start, c->tx_ring.end,
                c->tx_ring.head_ptr, c->tx_ring.tail_ptr,
                m)))
        {
            thread_yield();
        }
        writelen += clen;
        if (writelen == len)
        {
            return writelen;
        }
        else if (writelen > len)
        {
            log_err("Unmatched");
            return -EBADMSG;
        }
        else
            continue;
    }
    abort();
    return -EUNATCH;
}

static inline void cxltp_write_finish(cxltpconn_t *c, ssize_t writelen)
{
    if (writelen <= 0)
        return;
}

ssize_t cxltp_write(cxltpconn_t *c, const void *buf, size_t len)
{
    ssize_t writelen = cxltp_write_wait(c, len, buf);

    cxltp_write_finish(c, writelen);

    return writelen;
}

static inline ssize_t cxltp_do_write(cxltpconn_t *c, size_t len, const char *buf)
{
    const char *pos = buf;
    ssize_t writelen = 0;
    bool new_m = false;
    while (writelen < len)
    {
        if (can_write_payload(c->tx_ring.start, c->tx_ring.end,
                              c->tx_ring.head_ptr, c->tx_ring.tail_ptr) != 0)
        {
            return writelen;
        }
        void **cur_head = (void **)atomic64_read(c->tx_ring.head_ptr);
        struct cxltp_package_headr *m = NULL;
        if (*cur_head != NULL)
        {
            enum package_type type = ((struct cxltp_package_headr *)*cur_head)->type;
            if (unlikely(type == error_package))
            {
                return -103;
            }
            new_m = false;
            m = *cur_head;
        }
        else
        {
            new_m = true;
            m = tcache_alloc(&perthread_get(cxltp_tx_buf_pt));
        }
        if (!m)
        {
            return writelen;
        }
        m->head = 0;
        m->size = MSG_BUF_SIZE - sizeof(*m);
        char *m_base = (char *)(m + sizeof(*m));
        spin_lock_np(&c->l);

        if (unlikely(c->shutdown))
        {
            spin_unlock_np(&c->l);
            if (new_m)
                tcache_free(&perthread_get(cxltp_tx_buf_pt), m);
            return -103;
        }
        size_t clen = 0;
        clen = m->size > len - writelen ? len - writelen : m->size;
        memcpy(m_base, pos, clen);
        m->type = data_package;
        m->size = clen;
        spin_unlock_np(&c->l);
        /* block until there is an actionable event */
        if (unlikely(
                do_write_payload(
                    c->tx_ring.start, c->tx_ring.end,
                    c->tx_ring.head_ptr, c->tx_ring.tail_ptr,
                    m)))
        {
            if (new_m)
                tcache_free(&perthread_get(cxltp_tx_buf_pt), m);
            return writelen;
        }
        writelen += clen;
        if (writelen == len)
        {
            return writelen;
        }
        else if (writelen > len)
        {
            log_err("Unmatched");
            return -EBADMSG;
        }
        else
            continue;
    }
    abort();
    return -EUNATCH;
}

ssize_t cxltp_try_write(cxltpconn_t *c, const void *buf, size_t len)
{
    ssize_t writelen = cxltp_do_write(c, len, buf);
    return writelen;
}

ssize_t cxltp_writev(cxltpconn_t *c, const struct iovec *iov, int iovcnt)
{
    size_t winlen = MSG_BUF_SIZE - sizeof(struct cxltp_package_headr);
    ssize_t sent = 0;
    int i;

    char buf[winlen];
    /* actually send the data */
    for (i = 0; i < iovcnt; i++, iov++)
    {
        if (winlen <= 0 || winlen < iov->iov_len)
        {
            // todo handle unsent data
            break;
        }
        memcpy(buf + sent, iov->iov_base, iov->iov_len);
        sent += iov->iov_len;
        winlen -= iov->iov_len;
    }
    sent = cxltp_write_wait(c, sent, buf);
    /* catch up on any pending work */
    cxltp_write_finish(c, sent);

    return sent;
}

int send_rst(cxltpconn_t *c)
{
    void **cur_head = (void **)atomic64_read(c->tx_ring.head_ptr);
    struct cxltp_package_headr *m = NULL;
    if (*cur_head != NULL)
    {
        enum package_type type = ((struct cxltp_package_headr *)*cur_head)->type;
        if (unlikely(type == error_package))
        {
            return -103;
        }
        m = *cur_head;
    }
    else
    {
        m = tcache_alloc(&perthread_get(cxltp_tx_buf_pt));
    }
    if (!m)
    {
        log_info("NOMEM");
        return -ENOMEM;
    }
    m->type = error_package;
    while (unlikely(
        do_write_payload(
            c->tx_ring.start, c->tx_ring.end,
            c->tx_ring.head_ptr, c->tx_ring.tail_ptr,
            m)))
    {
        thread_yield();
    }
    return 0;
}

void cxltp_abort(cxltpconn_t *c)
{
    spin_lock_np(&c->l);
    if (c->shutdown)
    {
        spin_unlock_np(&c->l);
        return;
    }
    c->shutdown = true;
    spin_unlock_np(&c->l);
    send_rst(c);
    return;
}

void cxltp_close(cxltpconn_t *c)
{
    spin_lock_np(&c->l);
    if (c->shutdown)
    {
        spin_unlock_np(&c->l);
        return;
    }
    c->shutdown = true;
    spin_unlock_np(&c->l);
    send_rst(c);
}

int cxltp_shutdown(cxltpconn_t *c)
{
    int ret;
    log_debug("cxltp_shutdown");
    spin_lock_np(&c->l);
    if (c->shutdown)
    {
        spin_unlock_np(&c->l);
        return 0;
    }
    c->shutdown = true;
    spin_unlock_np(&c->l);

    ret = send_rst(c);
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}

void init_cxltp_queue_pool_meta(cxltp_queue_pool_meta_t *meta)
{
    CXLTP_SET(meta);
    spin_lock_init(&meta->alloced_lock);
    bitmap_init(meta->alloced, MSG_QUEUE_POOL_LENGTH, false);
}

void init_cxltp_ring_pool_meta(cxltp_ring_pool_meta_t *meta)
{
    log_debug("%p", meta);
    CXLTP_SET(meta);
    spin_lock_init(&meta->alloced_lock);
    bitmap_init(meta->alloced, MSG_RING_POOL_LENGTH, false);
}

int cxltp_init(void)
{
    int ret;

    if (!cfg_cxltp_enabled)
        return 0;

    void *mem = mem_map_shm(MSG_MEM_KEY, (void *)MSG_MEM_BASE, MSG_MEM_SIZE, PGSIZE_2MB, false);
    if (mem == MAP_FAILED || mem != (void *)MSG_MEM_BASE)
    {
        log_err("Fail to map shm for cxltp for %s", strerror(errno));
        return -ENOMEM;
    }
    touch_mapping(mem, MSG_MEM_SIZE, PGSIZE_2MB);
    cxltp_mem_meta_t *mem_meta = mem;
    size_t size;
    if (force_init_cxltp_mem)
    {
        log_info("Do force_init_cxltp_mem");
        atomic64_write(&mem_meta->magic, CXLTP_MAGIC);
        mem += sizeof(mem_meta);
        atomic64_write(&mem_meta->alloced, (unsigned long long)mem);
    }
    else if (atomic64_read(&mem_meta->magic) != CXLTP_MAGIC)
    {
        log_err("cxltp mem is not inited.");
        return -EINVAL;
    }
    assert(align_up(MSG_BUF_SIZE, PGSIZE_2MB) + MSG_RING_NUM * sizeof(cxltp_ring_data_entry_t) + align_up(sizeof(*global_cxltp_ring_pool), PGSIZE_2MB) < MSG_MEM_SIZE);
    mem = cxltp_mem_alloc(MSG_BUF_POOL_SIZE, &mem_meta->alloced);

    if (!mem)
        return -ENOMEM;

    ret = mempool_create(&cxltp_tx_buf_mp, mem, MSG_BUF_POOL_SIZE, PGSIZE_2MB,
                         MSG_BUF_SIZE);
    if (ret)
        return ret;
    cxltp_tx_buf_tcache = mempool_create_tcache(&cxltp_tx_buf_mp, "runtime_tx_bufs", TCACHE_DEFAULT_MAG_SIZE);
    if (!cxltp_tx_buf_tcache)
        return -ENOMEM;

    mem = cxltp_mem_alloc(PGSIZE_2MB, &mem_meta->alloced);
    BUG_ON(((uint64_t)mem) % PGSIZE_2MB != 0);
    log_info("safe mem is %p", mem);

    size = align_up(MSG_RING_NUM * sizeof(cxltp_ring_data_entry_t), PGSIZE_2MB);
    mem = cxltp_mem_alloc(size, &mem_meta->alloced);
    if (!mem)
        return -ENOMEM;
    log_info("ring mem is %p", mem);

    ret = mempool_create(&cxltp_tx_ring_buf_mp, mem, size, PGSIZE_2MB,
                         sizeof(cxltp_ring_data_entry_t));
    if (ret)
    {
        log_err("Fail to mempool_create in cxltp_init");
        return ret;
    }
    cxltp_tx_ring_buf_tcache = mempool_create_tcache(&cxltp_tx_ring_buf_mp, "runtime_tx_ring_bufs", TCACHE_DEFAULT_MAG_SIZE);
    if (!cxltp_tx_ring_buf_tcache)
    {
        log_err("Fail to mempool_create in cxltp_init");
        return -ENOMEM;
    }

    global_cxltp_queue_pool_meta = cxltp_mem_alloc(align_up(sizeof(*global_cxltp_queue_pool_meta), PGSIZE_2MB), &mem_meta->alloced);
    global_cxltp_ring_pool = cxltp_mem_alloc(align_up(sizeof(*global_cxltp_ring_pool), PGSIZE_2MB), &mem_meta->alloced);
    if (!global_cxltp_queue_pool_meta)
    {
        log_err("Fail to alloc global_cxltp_queue_pool_meta");
        return -ENOMEM;
    }
    if (!global_cxltp_ring_pool)
    {
        log_err("Fail to alloc global_cxltp_ring_pool");
        return -ENOMEM;
    }
    log_debug("before init_cxltp_queue_pool_meta.");
    init_cxltp_queue_pool_meta(global_cxltp_queue_pool_meta);
    init_cxltp_ring_pool_meta(global_cxltp_ring_pool);
    log_info("CXLTP init done");
    return 0;
}

struct waketqup_args
{
    void *k;
    void *q;
};

void waketqup(void *args)
{
    struct waketqup_args *wa = args;
    struct kthread *k = wa->k;
    tqueue_t *q = &k->cxltp_rx.tq;
    while (true)
    {
        if ((preempt_cnt & ~PREEMPT_NOT_PENDING) != 0)
            log_err("1 preempt_cnt: %x", preempt_cnt);
        rx_batch(q);
        if ((preempt_cnt & ~PREEMPT_NOT_PENDING) != 0)
            log_err("2 preempt_cnt: %x", preempt_cnt);
        preempt_disable();
        if ((preempt_cnt & ~PREEMPT_NOT_PENDING) != 1)
            log_err("3 preempt_cnt: %x", preempt_cnt);
        k->cxltp_busy = false;
        thread_park_and_preempt_enable();
    }
}

int cxltp_init_thread(void)
{
    if (!cfg_cxltp_enabled)
        return 0;
    tcache_init_perthread(cxltp_tx_buf_tcache, &perthread_get(cxltp_tx_buf_pt));
    tcache_init_perthread(cxltp_tx_ring_buf_tcache, &perthread_get(cxltp_tx_ring_buf_pt));
    tqueue_init(&perthread_get(perk_tq));
    // thread_spawn(waketqup, (void*)&perthread_get(perk_tq));

#ifdef VESSEL_CXLTP
    tqueue_init(&myk()->cxltp_rx.tq);
    struct waketqup_args *wa = malloc(sizeof(struct waketqup_args));
    wa->k = myk();
    wa->q = NULL;
    thread_t *th = thread_create(waketqup, (void *)wa);
    if (!th)
        return -ENOMEM;
    myk()->cxltp_softirq = th;
#endif
    myk()->cxltp_busy = false;

    struct thread_spec *ts = &iok.threads[myk()->kthread_idx];
    if (cfg_cxltp_enabled)
    {
        ts->cxltp_waiters = &myk()->cxltp_rx.tq;
    }
    else
    {
        ts->cxltp_waiters = NULL;
    }
    // if(myk()->kthread_idx==maxks-1){
    //     log_info("create one global_check");
    //     thread_spawn(global_check, NULL);
    // }
    return 0;
}
#else
int cxltp_init(void)
{
    return 0;
}
int cxltp_init_thread(void)
{
    return 0;
}
int cxltp_listen(cxltp_addr_t *cxltp_addr_arg, size_t backlog, cxltpqueue_t **q_out)
{
    return -1;
}

int cxltp_accept(cxltpqueue_t *q, cxltpconn_t **c_out)
{
    return -1;
};

int cxltp_dial(cxltp_addr_t cxltp_addr, cxltpconn_t **c_out)
{
    return -1;
};

ssize_t cxltp_try_read(cxltpconn_t *c, void *buf, size_t len)
{
    return -1;
};
ssize_t cxltp_try_write(cxltpconn_t *c, const void *buf, size_t len)
{
    return -1;
};

ssize_t cxltp_read(cxltpconn_t *c, void *buf, size_t len)
{
    return -1;
};
ssize_t cxltp_write(cxltpconn_t *c, const void *buf, size_t len)
{
    return -1;
};
void cxltp_abort(cxltpconn_t *c)
{
    return;
};
void cxltp_close(cxltpconn_t *c)
{
    return;
};
int cxltp_shutdown(cxltpconn_t *c)
{
    return -1;
};
#endif