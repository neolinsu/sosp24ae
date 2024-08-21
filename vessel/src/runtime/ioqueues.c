/*
 * ioqueues.c
 */

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <numaif.h>

#include <base/lock.h>
#include <base/hash.h>
#include <base/log.h>
#include <base/lrpc.h>
#include <base/mem.h>

#include <iokernel/shm.h>
#include <runtime/thread.h>
#include <runtime/minialloc.h>

#include <net/ethernet.h>
#include <net/mbuf.h>

#include "scheds/state.h"
#include <vessel/interface.h>
#include "defs.h"
#include "net/defs.h"

#define PACKET_QUEUE_MCOUNT	    4096
#define COMMAND_QUEUE_MCOUNT	4096

bool is_server=false;
/* the egress buffer pool must be large enough to fill all the TXQs entirely */
static size_t calculate_egress_pool_size(void)
{
	size_t buflen = MBUF_DEFAULT_LEN;
	return align_up(PACKET_QUEUE_MCOUNT *
			buflen * MAX(1, guaranteedks) * 8UL,
			PGSIZE_2MB);
}

struct iokernel_control iok;
bool cfg_prio_is_lc;
uint64_t cfg_ht_punish_us;
uint64_t cfg_qdelay_us = 10;

static int generate_random_mac(struct eth_addr *mac)
{
	int fd, ret;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, mac, sizeof(*mac));
	close(fd);
	if (ret != sizeof(*mac))
		return -1;

	mac->addr[0] &= ~ETH_ADDR_GROUP;
	mac->addr[0] |= ETH_ADDR_LOCAL_ADMIN;

	return 0;
}

// Could be a macro really, this is totally static :/
static size_t estimate_shm_space(void)
{
	size_t ret = 0, q;

	// Header + queue_spec information
	ret += sizeof(struct control_hdr);
	ret += sizeof(struct thread_spec) * maxks;
	ret = align_up(ret, CACHE_LINE_SIZE);

	// Compute congestion signal line
	ret += CACHE_LINE_SIZE;

	// RX queues (wb is not included)
	q = sizeof(struct lrpc_msg) * PACKET_QUEUE_MCOUNT;
	q = align_up(q, CACHE_LINE_SIZE);
	ret += q * maxks;

	// TX packet queues
	q = sizeof(struct lrpc_msg) * PACKET_QUEUE_MCOUNT;
	q = align_up(q, CACHE_LINE_SIZE);
	q += align_up(sizeof(uint32_t), CACHE_LINE_SIZE);
	ret += q * maxks;

	// TX command queues
	q = sizeof(struct lrpc_msg) * COMMAND_QUEUE_MCOUNT;
	q = align_up(q, CACHE_LINE_SIZE);
	q += align_up(sizeof(uint32_t), CACHE_LINE_SIZE);
	ret += q * maxks;

	// Shared queue pointers for the iokernel to use to determine busyness
	q = align_up(sizeof(struct q_ptrs), CACHE_LINE_SIZE);
	ret += q * maxks;

	ret = align_up(ret, PGSIZE_2MB);

	// Egress buffers
	BUILD_ASSERT(PGSIZE_2MB % MBUF_DEFAULT_LEN == 0);
	ret += calculate_egress_pool_size();
	ret = align_up(ret, PGSIZE_2MB);

	ret += align_up(BUSY_KTHREAD_SIZE * sizeof(atomic_t), CACHE_LINE_SIZE);
	ret += align_up(sizeof(struct lf_queue), CACHE_LINE_SIZE);

#ifdef DIRECTPATH
	// mlx5 directpath
	if (cfg_directpath_enabled)
		ret += PGSIZE_2MB * 4;
#endif

#ifdef DIRECT_STORAGE
	// SPDK completion queue memory
	if (cfg_storage_enabled) {
		/* sizeof(spdk_nvme_cpl) * default queue len * threads */
		ret += 16 * 4096 * maxks;
	}
#endif
	return ret;
}

/*
 * iok_shm_alloc - allocator for iokernel shared memory region
 * this is intended only for use during initialization.
 * panics if memory can't be allocated
 *
 */
void *iok_shm_alloc(size_t size, size_t alignment, shmptr_t *shm_out)
{
	static DEFINE_SPINLOCK(shmlock);
	static size_t allocated;
	struct shm_region *r = &netcfg.tx_region;
	void *p;

	spin_lock(&shmlock);
	if (!r->base) {
		r->len = estimate_shm_space();
		// Allocate mem from vessel data zone.
		r->base = mini_malloc(align_up(r->len, PGSIZE_2MB));
		memset(r->base, 0, r->len);
		iok.key=r->base;
		if (r->base == NULL)
			panic("failed to map shared memory (requested %lu bytes)", r->len);
	}

	if (alignment < CACHE_LINE_SIZE)
		alignment = CACHE_LINE_SIZE;

	allocated = align_up(allocated, alignment);
	p = shmptr_to_ptr(r, allocated, size);
	BUG_ON(!p);

	if (shm_out)
		*shm_out = allocated;

	allocated += size;

	spin_unlock(&shmlock);

	return p;
}

static void ioqueue_alloc(struct queue_spec *q, size_t msg_count,
			  bool alloc_wb)
{
	iok_shm_alloc(sizeof(struct lrpc_msg) * msg_count, CACHE_LINE_SIZE, &q->msg_buf);

	if (alloc_wb)
		iok_shm_alloc(CACHE_LINE_SIZE, CACHE_LINE_SIZE, &q->wb);

	q->msg_count = msg_count;
}

static void busy_kthread_alloc(struct lf_queue** q, size_t size) {
	atomic_t *meta = iok_shm_alloc(size * sizeof(atomic_t), CACHE_LINE_SIZE, NULL);
	for (int i=0; i<size; ++i) atomic_write(&meta[i], LF_QUEUE_EMPTY);
	struct lf_queue* myq = iok_shm_alloc(sizeof(struct lf_queue), CACHE_LINE_SIZE, NULL);
	lf_queue_init(myq, meta, size);
	*q = myq;
}

/*
 * General initialization for runtime <-> iokernel communication. Must be
 * called before per-thread ioqueues initialization.
 */
int ioqueues_init(void)
{
	bool has_mac = false;
	int i, ret;
	struct thread_spec *ts;
	

	for (i = 0; i < ARRAY_SIZE(netcfg.mac.addr); i++)
		has_mac |= netcfg.mac.addr[i] != 0;

	if (!has_mac) {
		ret = generate_random_mac(&netcfg.mac);
		if (ret < 0)
			return ret;
	}

	mem_key_t key = 0;
	char *vessel_name = getenv("VESSEL_NAME");
	if (vessel_name==NULL) {
		log_err("VESSEL_NAME not set.");
		abort();
	}
	if(strcmp("server", vessel_name) == 0) {
		key = INGRESS_MBUF_SHM_KEY;
		is_server = true;
	} else {
		key = INGRESS_MBUF_SHM_KEY_CLIENT;
		is_server = false;
	}
	/* map ingress memory */
	netcfg.rx_region.base =
	    mem_map_shm_rdonly(key, NULL, INGRESS_MBUF_SHM_SIZE,
			PGSIZE_2MB);
	if (netcfg.rx_region.base == MAP_FAILED) {
		log_err("control_setup: failed to map ingress region");
		log_err("Please make sure IOKernel is running");
		return -1;
	}
	netcfg.rx_region.len = INGRESS_MBUF_SHM_SIZE;
#if 0
	iok.iok_info = (struct iokernel_info *)netcfg.rx_region.base;
#endif

	/* set up queues in shared memory */
	iok.hdr = iok_shm_alloc(sizeof(*iok.hdr), 0, NULL);
	iok.threads = iok_shm_alloc(sizeof(*ts) * maxks, 0, NULL);
	runtime_congestion = iok_shm_alloc(sizeof(struct congestion_info),
					   0, &iok.hdr->congestion_info);

	for (i = 0; i < maxks; i++) {
		ts = &iok.threads[i];
		ioqueue_alloc(&ts->rxq, PACKET_QUEUE_MCOUNT, false);
		ioqueue_alloc(&ts->txpktq, PACKET_QUEUE_MCOUNT, true);
		ioqueue_alloc(&ts->txcmdq, COMMAND_QUEUE_MCOUNT, true);

		iok_shm_alloc(sizeof(struct q_ptrs), CACHE_LINE_SIZE, &ts->q_ptrs);
		iok_shm_alloc(sizeof(uint64_t), CACHE_LINE_SIZE, &ts->qdelay);
		ts->rxq.wb = ts->q_ptrs;
	}

	iok.tx_len = calculate_egress_pool_size();
	iok.tx_buf = iok_shm_alloc(iok.tx_len, PGSIZE_2MB, NULL);
	busy_kthread_alloc(&iok.busy_kthreads, BUSY_KTHREAD_SIZE);
	return 0;
}

static void ioqueues_shm_cleanup(void)
{
	mem_unmap_shm(netcfg.tx_region.base);
	mem_unmap_shm(netcfg.rx_region.base);
}

extern spinlock_t uipi_fd_l;
extern struct list_head yield_fd_list;
extern struct list_head cede_fd_list;
extern volatile int uipi_fd_cnt;

/*
 * Register this runtime with the IOKernel. All threads must complete their
 * per-thread ioqueues initialization before this function is called.
 */
int ioqueues_register_iokernel(void)
{
	struct control_hdr *hdr;
	struct shm_region *r = &netcfg.tx_region;
	struct sockaddr_un addr;
	int ret;

	/* initialize control header */
	hdr = iok.hdr;
	BUG_ON((uintptr_t)iok.hdr != (uintptr_t)r->base);
	hdr->magic = CONTROL_HDR_MAGIC;
	hdr->version_no = CONTROL_HDR_VERSION;
	/* TODO: overestimating is okay, but fix this later */
	hdr->egress_buf_count = div_up(iok.tx_len, net_get_mtu() + MBUF_HEAD_LEN);
	hdr->thread_count = maxks;
	hdr->mac = netcfg.mac;

	hdr->sched_cfg.priority = cfg_prio_is_lc ?
				  SCHED_PRIO_LC : SCHED_PRIO_BE;
	hdr->sched_cfg.ht_punish_us = cfg_ht_punish_us;
	hdr->sched_cfg.qdelay_us = cfg_qdelay_us;
	hdr->sched_cfg.max_cores = maxks;
	hdr->sched_cfg.guaranteed_cores = guaranteedks;
	hdr->sched_cfg.preferred_socket = preferred_socket;

	hdr->thread_specs = ptr_to_shmptr(r, iok.threads, sizeof(*iok.threads) * maxks);
	hdr->busy_kthreads = ptr_to_shmptr(r, iok.busy_kthreads, sizeof(*iok.busy_kthreads));
	hdr->busy_kthreads_meta = ptr_to_shmptr(r, iok.busy_kthreads->meta, BUSY_KTHREAD_SIZE * sizeof(atomic_t));

	/* register with iokernel */
	// BUILD_ASSERT(strlen(CONTROL_SOCK_PATH) <= sizeof(addr.sun_path) - 1);
	memset(&addr, 0x0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	if(is_server){
		strncpy(addr.sun_path, "/tmp/server.sock", sizeof(addr.sun_path) - 1);
	}
	else{
		strncpy(addr.sun_path, "/tmp/client.sock", sizeof(addr.sun_path) - 1);
	}

	iok.fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (iok.fd == -1) {
		log_err("register_iokernel: socket() failed [%s]", strerror(errno));
		goto fail;
	}
	if (connect(iok.fd, (struct sockaddr *)&addr,
		 sizeof(struct sockaddr_un)) == -1) {
		log_err("register_iokernel: connect() failed [%s]", strerror(errno));
		goto fail_close_fd;
	}

	ret = write(iok.fd, &iok.key, sizeof(iok.key));
	if (ret != sizeof(iok.key)) {
		log_err("register_iokernel: write() failed [%s]", strerror(errno));
		goto fail_close_fd;
	}

	ret = write(iok.fd, &netcfg.tx_region.len, sizeof(netcfg.tx_region.len));
	if (ret != sizeof(netcfg.tx_region.len)) {
		log_err("register_iokernel: write() failed [%s]", strerror(errno));
		goto fail_close_fd;
	}
#ifdef VESSEL_UIPI
	struct uipi_fd *yield_fd, * cede_fd;
	spin_lock(&uipi_fd_l);
	int cur_uipi_fd_cnt = uipi_fd_cnt; 
	ret = write(iok.fd, &cur_uipi_fd_cnt, sizeof(cur_uipi_fd_cnt));
	if (ret != sizeof(cur_uipi_fd_cnt)) {
		log_err("write()");
		abort();
	}
	do {
		yield_fd = list_pop(&yield_fd_list, struct uipi_fd, link);
		cede_fd  = list_pop(&cede_fd_list, struct uipi_fd, link);
		if (yield_fd==NULL) break;
		BUG_ON(yield_fd->core_id != cede_fd->core_id);
		ret = write(iok.fd, &yield_fd->core_id, sizeof(yield_fd->core_id));
		if (ret != sizeof(yield_fd->core_id)) {
			log_err("write()");
			abort();
		}
		log_info("yield_fd->fd: %d", yield_fd->fd);
		send_my_fd(iok.fd, yield_fd->fd);
		send_my_fd(iok.fd, cede_fd->fd);
	} while(yield_fd!=NULL);
	spin_unlock(&uipi_fd_l);
#endif // VESSEL_UIPI
	return 0;
fail_close_fd:
	close(iok.fd);
fail:
	ioqueues_shm_cleanup();
	return -errno;
}

int ioqueues_init_thread(void)
{
	int ret;
	pid_t tid = myk()->tid = vessel_cluster_id();
	struct shm_region *r = &netcfg.tx_region;
	struct thread_spec *ts = &iok.threads[myk()->kthread_idx];
	ts->tid = tid;

	ret = shm_init_lrpc_in(r, &ts->rxq, &myk()->rxq);
	BUG_ON(ret);

	ret = shm_init_lrpc_out(r, &ts->txpktq, &myk()->txpktq);
	BUG_ON(ret);

	ret = shm_init_lrpc_out(r, &ts->txcmdq, &myk()->txcmdq);
	BUG_ON(ret);

	myk()->q_ptrs = (struct q_ptrs *) shmptr_to_ptr(r, ts->q_ptrs,
			sizeof(uint32_t));
	BUG_ON(!myk()->q_ptrs);
	myk()->qdelay = (uint64_t *) shmptr_to_ptr(r, ts->qdelay,
			sizeof(uint64_t));
	BUG_ON(!myk()->qdelay);

	return 0;
}
