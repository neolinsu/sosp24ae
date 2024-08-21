#include <numaif.h>
#include <base/stddef.h>
#include <base/mem.h>

struct cxltpconn;
#include "../runtime/cxl/tqueue.h"
#include "../runtime/cxl/cxltp_defs.h"
#define ONE_REQ_LAT 1
///////////////////////////////////////
struct iokernel_cfg {
	bool	noht; /* disable hyperthreads */
	bool	nobw; /* disable bandwidth controller */
	bool	noidlefastwake; /* disable fast wakeups for idle processes */
	bool	ias_prefer_selfpair; /* prefer self-pairings */
	float	ias_bw_limit; /* IAS bw limit, (MB/s) */
	bool	no_hw_qdel; /* Disable use of hardware timestamps for qdelay */
	int cxl_node_id;
};
extern struct iokernel_cfg cfg;
///////////////////////////////////////

uint64_t watch_cxltp_tqueue(void* q, uint64_t bound_us) {
 	uint64_t ret = 0;
    tqueue_t *tq = q;
	struct tqueue_elem *te, *nte;
    uint64_t bound = bound_us / ONE_REQ_LAT;
    bool locked = spin_try_lock(&tq->lock);
    if (!locked) return 0;
	list_for_each_safe(&tq->waiters, te, nte, n) {
		cxltpconn_t *c = te->c;
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr) || 
			c->shutdown) {
			ret++;
            if (unlikely(ret > bound)) {
                break;
            }
		}
	}
    if (ret!=0) {
        tq->state = TQUEUE_HASQ;
    }
    spin_unlock(&tq->lock);
    return ret * ONE_REQ_LAT * cycles_per_us;
}

int cxltp_probe_init(void) {
    int ret;
    unsigned long mask = 1 << cfg.cxl_node_id;
    void *mem = mem_map_shm(MSG_MEM_KEY, (void*)MSG_MEM_BASE, MSG_MEM_SIZE, PGSIZE_2MB, false);
    
    if (mem == MAP_FAILED || mem != (void*)MSG_MEM_BASE) {
        log_err("Fail to map shm for cxltp for %s", strerror(errno));
        return -ENOMEM;
    }

    ret = mbind(mem, MSG_MEM_SIZE, MPOL_BIND, &mask, 64, 0);
    if (ret) {
        log_err ("Fail to mbind in cxltp for %s", strerror(errno));
        return ret;
    }

    for (char *pos = (char *)mem; pos < (char *)mem + MSG_MEM_SIZE; pos += PGSIZE_2MB)
		ACCESS_ONCE(*pos);
    return 0;
}