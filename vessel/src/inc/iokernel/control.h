/*
 * control.h - the control interface for the I/O kernel
 */

#pragma once

#include <sys/types.h>

#include <base/bitmap.h>
#include <base/limits.h>
#include <base/lf_queue.h>
#include <iokernel/shm.h>
#include <net/ethernet.h>

#define BUSY_KTHREAD_SIZE 128
/*
 * WARNING: If you make any changes that impact the layout of
 * struct control_hdr, please increment the version number!
 */

#define CONTROL_HDR_VERSION 5

/* The abstract namespace path for the control socket. */
#define CONTROL_SOCK_PATH	"/tmp/server.sock"

/* describes a queue */
struct q_ptrs {
	uint32_t		rxq_wb; /* must be first */
	uint32_t		rq_head;
	uint32_t		rq_tail;
	uint32_t		directpath_rx_tail;
	uint64_t		next_timer_tsc;
	uint32_t		storage_tail;
	uint32_t		pad;
	uint64_t		oldest_tsc;
	uint64_t		rcu_gen;
	uint64_t		run_start_tsc;
};

BUILD_ASSERT(sizeof(struct q_ptrs) <= CACHE_LINE_SIZE);

struct congestion_info {
	float			load;
	uint64_t		delay_us;
	uint32_t		kthread_yield_requested;
	uint64_t		avg_delay_tsc;
};

enum {
	HWQ_INVALID = 0,
	HWQ_MLX5,
	HWQ_SPDK_NVME,
	HWQ_MLX5_QSTEERING,
	HWQ_CXLTP,
	NR_HWQ,
};

struct hardware_queue_spec {
	shmptr_t		descriptor_table;
	shmptr_t		consumer_idx;
	uint32_t		descriptor_log_size;
	uint32_t		nr_descriptors;
	uint32_t		parity_byte_offset;
	uint32_t		parity_bit_mask;
	uint32_t		hwq_type;
};

struct timer_spec {
	shmptr_t		next_tsc;
	unsigned long		timer_resolution;
};


/* describes a runtime kernel thread */
struct thread_spec {
	struct queue_spec	rxq;
	struct queue_spec	txpktq;
	struct queue_spec	txcmdq;
	shmptr_t		q_ptrs;
	pid_t			tid;
	int32_t			park_efd;

	struct hardware_queue_spec	direct_rxq;
	struct hardware_queue_spec	storage_hwq;
	struct timer_spec		timer_heap;
	void *cxltp_waiters;
	shmptr_t qdelay;
};

enum {
	SCHED_PRIO_LC = 0, /* high priority, latency-critical task */
	SCHED_PRIO_BE,     /* low priority, best-effort task */
};

/* describes scheduler options */
struct sched_spec {
	unsigned int		priority;
	unsigned int		max_cores;
	unsigned int		guaranteed_cores;
	unsigned int		preferred_socket;
	uint64_t		qdelay_us;
	uint64_t		ht_punish_us;
};

#define CONTROL_HDR_MAGIC	0x696f6b3a /* "iok:" */

/* the main control header */
struct control_hdr {
	unsigned int		version_no;
	unsigned int		magic;
	unsigned int		thread_count;
	unsigned long		egress_buf_count;
	shmptr_t		congestion_info;
	struct eth_addr		mac;
	struct sched_spec	sched_cfg;
	shmptr_t		thread_specs;
	shmptr_t        busy_kthreads;
	shmptr_t        busy_kthreads_meta;
};

/* information shared from iokernel to all runtimes */
struct iokernel_info {
	DEFINE_BITMAP(managed_cores, NCPU);
	unsigned char rss_key[40];
};
