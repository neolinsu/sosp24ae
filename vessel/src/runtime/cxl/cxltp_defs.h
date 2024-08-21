#pragma once
#include <base/stddef.h>
#include <base/lock.h>
#include <base/bitmap.h>
#include <base/atomic.h>
#include <base/kref.h>

#include <runtime/cxl/cxltp.h>
#include <runtime/smalloc.h>

#include "waitq.h"
#include "tqueue.h"

#define MSG_MEM_KEY           (1984)
#define MSG_MEM_BASE          (0x2580000000llu)
#define MSG_MEM_SIZE          (512lu*MB)

BUILD_ASSERT(MSG_MEM_BASE % PGSIZE_2MB == 0);
BUILD_ASSERT(MSG_MEM_SIZE % PGSIZE_2MB == 0);

#define MSG_BUF_SIZE          (2048lu)
#define MSG_BUF_POOL_LENGTH   (32*1024lu)
#define MSG_BUF_POOL_SIZE     (MSG_BUF_POOL_LENGTH * MSG_BUF_SIZE)


#define MSG_RING_POOL_LENGTH  (1024)
#define MSG_RING_LENGTH       (32)
#define MSG_RING_NUM          (1024)
#define MSG_QUEUE_POOL_LENGTH (1024)


#define MSG_RING_BUF_LENGTH   (16)
#define MSG_DAIL_RETRY        (100000)
#define MSG_LISTEN_RETRY      (100)
#define MSG_LISTEM_INTERV     (1)

#define CXLTP_MAGIC             (0xF132422ull)

#define SET_INDEX(index, value) (ACCESS_ONCE(*(index))) = (uint64_t) value 

#define CXLTP_IS_POOL(pool_ptr) ((pool_ptr)->magic==CXLTP_MAGIC)
#define CXLTP_SET(pool_ptr) (pool_ptr)->magic=CXLTP_MAGIC

struct cxltp_mem_meta {
    atomic64_t magic;
    atomic64_t alloced;
};
typedef struct cxltp_mem_meta cxltp_mem_meta_t;

struct cxltp_ring_pool_meta;

struct cxltp_ring_meta {
    void * start;
    void * end;
    atomic64_t *head_ptr;
    atomic64_t *tail_ptr;
    uint64_t timeout;
    void* pool;
    int id;
    uint8_t pad[16-sizeof(int)];
};
typedef struct cxltp_ring_meta cxltp_ring_meta_t;
BUILD_ASSERT(sizeof(cxltp_ring_meta_t) == CACHE_LINE_SIZE);

enum package_type {
    error_package,
    data_package,
    abort_package,
    to_free_package,
};

struct cxltp_package_headr {
    enum package_type type;
    size_t head;
    size_t size;
};

struct package_link {
    struct cxltp_package_headr *m;
    struct list_node link;
};

enum {
    client_state_idle,
    client_state_taken,
    client_state_ready,
    client_state_done
};

enum {
    server_state_idle,
    server_state_ready
};

struct cxltpqueue {
	spinlock_t       l;
	struct list_head conns;
    waitq_t          wq;
	bool             shutdown;
    size_t           backlog;
    
	spinlock_t       cl;
    atomic64_t       client_state;
    atomic64_t       server_state;
    cxltp_ring_meta_t  client_ring_meta;
    cxltp_ring_meta_t  server_ring_meta;

	struct kref                ref;
	struct link_pool_status    status;
    struct cxltp_ring_pool_meta *ring_pool;

    void *pool;
};
typedef struct cxltpqueue cxltpqueue_t;

struct cxltpconn {
    // Cache Line 1
    cxltp_ring_meta_t tx_ring;
    // Cache Line 2
    cxltp_ring_meta_t rx_ring;
    // Cache Line 3
    struct list_node queue_link;
    spinlock_t l;
    bool shutdown;
    struct kref ref;
    uint8_t pad1[CACHE_LINE_SIZE 
                -sizeof(struct list_node)
                -sizeof(spinlock_t)
                -8 // sizeof(bool)
                -sizeof(struct kref)];
    // Cache Line 4
    spinlock_t *rx_l_ptr;
    spinlock_t *tx_l_ptr;
    uint8_t pad2[CACHE_LINE_SIZE
                -sizeof(spinlock_t*) * 2
                ];
    uint64_t start;
    uint64_t sum;
    uint64_t cnt;
    uint8_t pad3[CACHE_LINE_SIZE
                -sizeof(uint64_t) * 3
                ];
};

typedef struct cxltpconn cxltpconn_t;

BUILD_ASSERT(sizeof(cxltpconn_t) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(cxltpconn_t, rx_ring)    % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(cxltpconn_t, queue_link) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(cxltpconn_t, rx_l_ptr)   % CACHE_LINE_SIZE == 0);


//////////////////////////////////////////////////////////////
/// On CXL Fabric

struct cxltp_ring_data_entry {
    uint64_t payload[MSG_RING_LENGTH];
};
typedef struct cxltp_ring_data_entry cxltp_ring_data_entry_t;

struct cxltp_ring_pool_meta {
    uint64_t magic;
    spinlock_t alloced_lock;
    DEFINE_BITMAP(alloced, MSG_RING_POOL_LENGTH);
    atomic64_t  heads[MSG_RING_POOL_LENGTH];
    atomic64_t  tails[MSG_RING_POOL_LENGTH];
    void    *starts[MSG_RING_POOL_LENGTH];
    void      *ends[MSG_RING_POOL_LENGTH];
};
typedef struct cxltp_ring_pool_meta cxltp_ring_pool_meta_t;

struct cxltp_queue_pool_meta {
    uint64_t magic;
    spinlock_t alloced_lock;
    DEFINE_BITMAP(alloced, MSG_QUEUE_POOL_LENGTH);
    cxltpqueue_t queues[MSG_QUEUE_POOL_LENGTH];
};
typedef struct cxltp_queue_pool_meta cxltp_queue_pool_meta_t;
//////////////////////////////////////////////////////////////

static inline void* cxltp_mem_alloc(size_t len, atomic64_t *alloced) {
    bool ret = false;
    uint64_t alloc, new, new_head;
    while(!ret) {
        alloc    = atomic64_read(alloced);
        log_debug("alloc:%p", (void*)alloc);
        new_head = align_up(alloc, PGSIZE_2MB);
        len      = align_up(len, PGSIZE_2MB);
        if(new_head + len > MSG_MEM_BASE + MSG_MEM_SIZE ) {
            return NULL;
        }
        new = new_head + len;
        ret = atomic64_cmpxchg(alloced, alloc, new);
    }
    return (void*) new_head;
}

static inline void log_cxltp_addr(cxltp_addr_t * addr) {
    log_info("base:%p, port:%lu", addr->base, addr->port);
}
