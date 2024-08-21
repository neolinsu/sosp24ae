#include <errno.h>
#include <string.h>

#include "base/log.h"
#include "base/list.h"
#include "base/lock.h"
#include "base/tcb.h"
#include "core/mem.h"
#include "core/kthread.h"
#include "core/task.h"
#include "core/cluster.h"
#include "mpk/mpk.h"
#include "vcontext/vcontext.h"
#include "scheds/state.h"

typedef void (*runtime_fn_t)(void);

extern cpu_pkru_map_t     *global_cpu_pkru_map;
extern cluster_ctx_map_t  *global_cluster_ctx_map;
extern minimal_ops_map_t  *global_minimal_ops_map;
extern void* cluster_id2cluster_ptr[NCLUSTER];
extern spinlock_t id_cnt_l;
extern void __switch_to_user_via_ret(vcontext_t *ctx) __noreturn;
extern void __switch_to_user_via_ret_wo_fs(vcontext_t *ctx) __noreturn;
extern void init_switch_to(vcontext_t *ctx) __noreturn;
extern void __save_jmp(vcontext_t *ctx, runtime_fn_t fn, void* stack);

static void save_jmp(vcontext_t *ctx, runtime_fn_t fn)
{
    __save_jmp(ctx, fn, my_kthread_stack);
}

#define RSP_ALIGNMENT	16
static inline void assert_rsp_aligned(uint64_t rsp)
{
	/*
	 * The stack must be 16-byte aligned at process entry according to
	 * the System V Application Binary Interface (section 3.4.1).
	 *
	 * The callee assumes a return address has been pushed on the aligned
	 * stack by CALL, so we look for an 8 byte offset.
	 */
	if(rsp % RSP_ALIGNMENT != sizeof(void *)) {
        log_err("assert_rsp_aligned failed!");
        abort();
    }
}


static __noreturn __noinline void csched(void) {
    cluster_ctx_t *next, *cur = (cluster_ctx_t *) cur_cluster;
again:
    if(ACCESS_ONCE(perc_core_conn->gen)!=perc_core_conn->last_gen) {
        while (ACCESS_ONCE(perc_core_conn->next_tid) == 0) {
            ACCESS_ONCE(perc_core_conn->last_gen) = perc_core_conn->gen;
            atomic_write(&perc_core_conn->idling, true);
            while(ACCESS_ONCE(perc_core_conn->gen) == perc_core_conn->last_gen) {
                cpu_relax();
            }
        }
        atomic_write(&perc_core_conn->idling, false);
        next = cluster_id2cluster_ptr[ACCESS_ONCE(perc_core_conn->next_tid)];
        while(!atomic_cmpxchg(&next->parked, true, false)) {
            cpu_relax();
        }
        //while(kthread_deattch_cluster(next)==-EAGAIN) {}; // Go on or waiting?
        ACCESS_ONCE(perc_core_conn->last_gen) = perc_core_conn->gen;
        goto have_next;
    }
    next = kthread_pop_cluster(my_kthread_meta);
have_next:
    if (likely(cur) && unlikely(cur->to_exit)) {
        int ret = EAGAIN;
        while (ret == EAGAIN) {
            ret = task_deattach_cluster(cur);
        }
        dealloc_cluster(cur);
        cur = NULL;
    }
    if (next) {
        if (unlikely(next->to_start)) {
            savefs(&(next->fctx.FS));
            save_envs((void*)(next->fctx.fpstate), (void*)&(next->fctx.xmstate));
        }
        cur = next;
        cur_cluster = cur;
        task_t *cur_task = (task_t *) cur->mytask;
        ACCESS_ONCE(global_cpu_pkru_map->map[my_kthread_meta->cpuid]) = key2pkru(cur_task->mpk);
        ACCESS_ONCE(global_cluster_ctx_map->map[my_kthread_meta->cpuid]) = cur;
        ACCESS_ONCE(global_minimal_ops_map->map[my_kthread_meta->cpuid]) = &(cur->minimal_ops);
    }
    if (likely(cur)) {
        if (likely(cur->to_start==false)) {
            // assert_rsp_aligned(cur->fctx.RSP);
            if (likely(!cur->with_fs)) {
                __switch_to_user_via_ret_wo_fs(&(cur->fctx));
            } else {
                //log_info("next->tid:%d", next->id);
                cur->with_fs = 0;
                __switch_to_user_via_ret(&(cur->fctx));
            }
        }
        else {
            next->to_start = false;
            init_switch_to(&cur->fctx);
        }
    } else {
        goto again;
    }
}

static void __mwait(void) {
    cluster_ctx_t *cur = (cluster_ctx_t*) cur_cluster;
    kthread_park_cluster(cur);
    atomic_write(&cur->parked, true);
    cur_cluster = NULL;
    atomic_write(&perc_core_conn->idling, true);
    //ACCESS_ONCE(perc_core_conn->idling) = true;
    while(ACCESS_ONCE(perc_core_conn->gen) == perc_core_conn->last_gen) {
        cpu_relax();
    }
    atomic_write(&perc_core_conn->idling, false);
    // ACCESS_ONCE(perc_core_conn->idling) = false;
    csched();
}

void cluster_mwait(void) {
    save_jmp(&(((cluster_ctx_t*)cur_cluster)->fctx), __mwait);
}

int cluster_id(void) {
    cluster_ctx_t *cur = (cluster_ctx_t*) cur_cluster;
    return cur->id;
}

static void __yield() {
    cluster_ctx_t *cur = (cluster_ctx_t*) cur_cluster;
    kthread_park_cluster(cur);
    atomic_write(&cur->parked, true);
    atomic_write(&perc_core_conn->idling, true);
    // ACCESS_ONCE(perc_core_conn->idling) = true;
    cur_cluster = NULL;
    csched();
}

void cluster_yield() {
    if (likely(cur_cluster)) {
        save_jmp(&(((cluster_ctx_t*)cur_cluster)->fctx), __yield);
    } else {
        csched();
    }
    return;
}

void cluster_exit() {
    cluster_ctx_t *cur = (cluster_ctx_t*) cur_cluster;
    cur->to_exit = true;
    csched();
    log_err("cluster_exit -> wrong branch");
    abort();
}

void* cluster_spawn(func_t func, void* args, int pref_core_id) {
    void* ret = NULL;
    cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
    task_t *cur_task = (task_t *) cur->mytask;
    if (!bitmap_test(cur_task->cpu_info.cpu_mask, pref_core_id)) {
        log_err("not set %d", pref_core_id);
        errno = EINVAL;
        goto out;
    }
    cluster_ctx_t *newc = alloc_cluster();
	char *news = (char *) (2047 * PAGE_SIZE +  (char *) ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(32, 2048 * PAGE_SIZE)) - 8;
    void* tcbp = cur_task->tls_ops.allocate_tls_storage();
#ifdef VESSEL_GLIBC2_34
    cur_task->tls_ops.tls_init(tcbp, true);
#else
    cur_task->tls_ops.tls_init(tcbp);
#endif
    tcbhead_t* cur_head = (void*)(cur->fctx.FS);
    tcbhead_t* head = tcbp;
    log_debug("cluster fs: %p, cede_cnt@%p", head, &newc->cede_cnt);
    head->tcb = tcbp;
    head->self = tcbp;
    head->feature_1 = cur_head->feature_1;
    head->pointer_guard = cur_head->pointer_guard;
    set_software_entry(newc, func, args, news, tcbp);
    newc->mytask = cur_task;
    bitmap_init(newc->cpu_affinity, NCPU, false);
    bitmap_or(newc->cpu_affinity, newc->cpu_affinity, cur_task->cpu_info.cpu_mask, NCPU);
    save_envs((void*)(newc->fctx.fpstate), (void*)&(newc->fctx.xmstate));
    newc->minimal_ops = cur->minimal_ops;
    newc->minimal_ops_je = cur->minimal_ops_je;
    newc->miniop_cnt_ptr = cur->miniop_cnt_ptr;
    newc->miniop_l_ptr = cur->miniop_l_ptr;
    newc->miniop_base_ptr = cur->miniop_base_ptr;
    newc->miniop_last_ptr = cur->miniop_last_ptr;
    newc->to_start = false;
    newc->with_fs = true;
    atomic_write(&newc->parked, false);


    while(task_attach_cluster(cur_task, newc)!=0);
    kthread_meta_t * pref_kthread_meta = get_kthread_meta_from_cpuid(pref_core_id);
    while(kthread_attch_cluster(pref_kthread_meta, newc)!=0);
    ret = head;
out:
    return ret;
}
