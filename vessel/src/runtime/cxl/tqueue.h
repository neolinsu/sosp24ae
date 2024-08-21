#pragma once
#include <base/list.h>
#include <base/lock.h>
#include <runtime/tls/thread.h>
#include <runtime/smalloc.h>

#include "./cxltp_defs.h"
#include "../defs.h"


#define TQUEUE_UNKOWN 1
#define TQUEUE_HASQ   2

static __always_inline bool has_req(atomic64_t * head, atomic64_t * tail) {
	return atomic64_read(head) != atomic64_read(tail);
}
static __always_inline void prefetch_next(struct tqueue_elem *nte) {
	cxltpconn_t *nc = nte->c;
	void* hp = nc->rx_ring.head_ptr; 
	void* tp = nc->rx_ring.tail_ptr;
	prefetch((hp));
	prefetch((tp));
}

static inline bool tq_has_req(tqueue_t *tq) {
	bool ret;
	struct tqueue_elem *te, *nte;
	ret = spin_try_lock(&tq->lock);
	if (!ret) goto out;

	if (tq->state == TQUEUE_HASQ) ret = true;
	list_for_each_safe(&tq->waiters, te, nte, n) {
		cxltpconn_t *c = te->c;
		if(list_node_from_off_(nte,list_off_var_(te, n)) != &(tq->waiters.n))
			prefetch_next(nte);
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr) || 
			c->shutdown) {
			ret = true;
			tq->state = TQUEUE_HASQ;
			goto unlock;
		}
	}
	ret = false;
unlock:
	spin_unlock(&tq->lock);
out:
	return ret;
}

static inline void tq_wait(tqueue_t *tq, cxltpconn_t *c) {
	assert_spin_lock_held(&c->l);
again:
	struct tqueue_elem *te = smalloc(sizeof(struct tqueue_elem));
	if (!te) {
		log_err("wait fail for no mem");
		spin_unlock_np(&c->l);
		thread_yield();
		spin_lock_np(&c->l);
		goto again;
	}
	te->c = c;
	te->th = thread_self();
	spin_lock(&tq->lock);
	list_add_tail(&tq->waiters, &te->n);
	spin_unlock(&tq->lock);

	thread_park_and_unlock_np(&c->l);
	sfree(te);
	spin_lock_np(&c->l);
}

static __always_inline void rx_batch(tqueue_t *tq) {
	struct tqueue_elem *te, *nte;
	spin_lock(&tq->lock);
	list_for_each_safe(&tq->waiters, te, nte, n) {
		cxltpconn_t *c = te->c;
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr)
		|| c->shutdown) {
			if(list_node_from_off_(nte,list_off_var_(te, n)) != &(tq->waiters.n))
				prefetch_next(nte);
			thread_ready(te->th);
			list_del_from(&tq->waiters, &te->n);
		}
	}
	tq->state = TQUEUE_UNKOWN;
	spin_unlock(&tq->lock);
}
static inline int tqueue_init(tqueue_t *q) {
	list_head_init(&q->waiters);
	spin_lock_init(&q->lock);
	q->state = TQUEUE_UNKOWN;
	return 0;
}

#if 0
static __always_inline void self_check(tqueue_t *q, struct list_node *conn_queue_link) {
	spin_lock_np(&q->l);
	list_add_tail(&q->waiters, &thread_self()->link);
    list_add_tail(&q->conns, conn_queue_link);
	q->cnt++;
	thread_t *th, *nextth;
	cmpconn_t *c, *nextc;
	int tail=0, cnt=0;
	uint64_t watchdog = 0;
	bool find = false;
	while(true) {
		tail=0; cnt=0;
		find = false;
		list_for_each_safe(&q->conns, c, nextc, queue_link) {
			if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr)) {
				list_del_from(&q->conns, &c->queue_link);
				find = true;
				ACCESS_ONCE(q->cnt)= q->cnt - 1;
				break;
			}
			tail++;
		}
		if (find) {
			list_for_each_safe(&q->waiters, th, nextth, link) {
				if (cnt==tail) {
					list_del_from(&q->waiters, &th->link);
					if (th!=__self) {
						thread_ready_head(th);
						thread_park_and_unlock_np(&q->l);
						return;
					} else {
						spin_unlock_np(&q->l);
						return;
					}
				}
				cnt++;
			}
			abort();
		}
		// thread_park_and_unlock_np(&q->l);
		// return;
		watchdog ++;
		if (unlikely(watchdog % 100000==0)) {
			if(ACCESS_ONCE(myk()->timern) > 0 &&
				ACCESS_ONCE(myk()->timers[0].deadline_us) <= microtime()) {
				thread_park_and_unlock_np(&q->l);
				return;
			}
			if(unlikely(c->shutdown)) {
				spin_unlock_np(&q->l);
				return;
			}
		}
		if (ACCESS_ONCE(myk()->rq_tail) != ACCESS_ONCE(myk()->rq_head) || 
			!list_empty(&myk()->rq_overflow)) {
			if (q->cnt>=3) {
				list_del_from(&q->waiters, &thread_self()->link);
				list_del_from(&q->conns, conn_queue_link);
				ACCESS_ONCE(q->cnt)=q->cnt-1;
				spin_unlock_np(&q->l);
				thread_yield();
				return;
			} else {
				thread_park_and_unlock_np(&q->l);
				return;
			}
		}
	}
}

static __always_inline void enqueue(tqueue_t *q, struct list_node *conn_queue_link) {
	spin_lock_np(&q->l);
	list_add_tail(&q->waiters, &thread_self()->link);
    list_add_tail(&q->conns, conn_queue_link);
	q->cnt++;
	thread_park_and_unlock_np(&q->l);
}

static __always_inline void dequeue(tqueue_t *q) {
	thread_t *th, *nextth;
	cmpconn_t *c, *nextc;
	int buf[1], tail=0, cnt=0, index;
	spin_lock_np(&q->l);
	tail=0, cnt=0, index=0;
	list_for_each_safe(&q->conns, c, nextc, queue_link) {
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr)) {
			list_del_from(&q->conns, &c->queue_link);
			buf[tail++] = cnt;
		}
		cnt++;
		if (tail>=1) break;
	}
	cnt = 0, index = 0;
	if(likely(tail==0)) goto out;
	list_for_each_safe(&q->waiters, th, nextth, link) {
		if (index>=tail) break;
		if (buf[index] == cnt) {
			list_del_from(&q->waiters, &th->link);
			q->cnt--;
			thread_ready(th);
			index++;
		}
		cnt++;
	}
out:
	spin_unlock_np(&q->l);
}

static inline int slow_dequeue(tqueue_t *q) {
	thread_t *th, *nextth;
	cmpconn_t *c, *nextc;
	int buf[32], tail=0, cnt=0, index;
	spin_lock_np(&q->l);
	list_for_each_safe(&q->conns, c, nextc, queue_link) {
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr) ||
			c->shutdown) {
			list_del_from(&q->conns, &c->queue_link);
			buf[tail++] = cnt;
		}
		cnt++;
		if (tail>=32) break;
	}
	cnt = 0, index = 0;
	if(likely(tail==0)) goto out;
	list_for_each_safe(&q->waiters, th, nextth, link) {
		if (index>=tail) break;
		if (buf[index] == cnt){
			list_del_from(&q->waiters, &th->link);
			thread_ready(th);
			index++;
		}
		cnt++;
	}
out:
	spin_unlock_np(&q->l);
	return tail;
}

static __always_inline int update_ready(tqueue_t *q) {
	thread_t *th, *nextth;
	cmpconn_t *c, *nextc;
	int buf[1], tail=0, cnt=0, index;
	int ret=spin_try_lock_np(&q->l);
	if (ret==false) {
        return -EAGAIN;
    }
	list_for_each_safe(&q->conns, c, nextc, queue_link) {
		if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr) ||
			c->shutdown) {
			list_del_from(&q->conns, &c->queue_link);
			buf[tail++] = cnt;
		}
		cnt++;
		if (tail>=1) break;
	}
	cnt = 0, index = 0;
	if(likely(tail==0)) goto out;
	list_for_each_safe(&q->waiters, th, nextth, link) {
		if (index>=tail) break;
		if (buf[index] == cnt){
			list_del_from(&q->waiters, &th->link);
			list_add_tail(&q->ready, &th->link);
			index++;
		}
		cnt++;
	}
	if(tail>0) {
        ACCESS_ONCE(q->k->cmp_wait) = true;
	}
out:
	spin_unlock_np(&q->l);
	return tail;
}

static __always_inline void do_ready(tqueue_t *q) {
	thread_t *th, *nextth;
	int to_free_num = 0;
	spin_lock_np(&q->l);
	list_for_each_safe(&q->ready, th, nextth, link) {
		list_del_from(&q->ready, &th->link);
		thread_ready(th);
		q->cnt--;
	}
	while (unlikely(q->cnt>CMP_HEAVY_MAX)) {
		th = list_pop(&q->waiters, thread_t, link);
		list_pop(&q->conns, cmpconn_t, queue_link);
		thread_ready(th);
		q->cnt--;
	}
	ACCESS_ONCE(q->k->cmp_wait) = false;
	spin_unlock_np(&q->l);
}

static __always_inline void spin_for_msg(struct kthread *k, tqueue_t *q) {
	thread_t *th, *nextth;
	cmpconn_t *c, *nextc;
	int buf[16], tail=0, cnt=0, index;
	uint64_t start_tsc;
	spin_lock_np(&q->l);
	while(true) {
		tail=0, cnt=0, index=0;
		list_for_each_safe(&q->conns, c, nextc, queue_link) {
			if (has_req(c->rx_ring.head_ptr, c->rx_ring.tail_ptr)
				||c->shutdown) {
				list_del_from(&q->conns, &c->queue_link);
				buf[tail++] = cnt;
			}
			cnt++;
			if (tail>=16) break;
		}
		cnt = 0, index = 0;
		if(likely(tail==0)) goto out;
		list_for_each_safe(&q->waiters, th, nextth, link) {
			if (index>=tail) break;
			if (buf[index] == cnt) {
				list_del_from(&q->waiters, &th->link);
				q->cnt--;
				thread_ready(th);
				index++;
			}
			cnt++;
		}
		goto out;
	}
out:
	spin_unlock_np(&q->l);
}

static __always_inline void do_one_ready(tqueue_t *q) {
	thread_t *th;
	spin_lock_np(&q->l);
	th = list_pop(&q->ready, thread_t, link);
	thread_ready(th);
	ACCESS_ONCE(q->k->cmp_wait) = !list_empty(&q->ready);
	q->cnt--;
	spin_unlock_np(&q->l);
}

static inline int tqueue_init(tqueue_t *q) {
	list_head_init(&q->conns);
	list_head_init(&q->waiters);
	list_head_init(&q->ready);
	spin_lock_init(&q->l);
	q->k = myk();
	q->cnt = 0;
	return 0;
}
#endif
