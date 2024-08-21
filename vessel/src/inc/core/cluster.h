#pragma once
#include <stddef.h>
#include <unistd.h>

#include "vcontext/vcontext.h"

#include "base/bitmap.h"
#include "base/lock.h"
#include "base/list.h"
#include "base/cpu.h"
#include "base/limits.h"

#include "core/mem.h"
#include "core/uipi.h"

typedef void(*func_t)(void*);

struct cluster_ctx {
    pid_t  id;
    struct list_node task_link;
    struct list_node kthread_link;
    void* mytask;
    void* uipi_signal_stack;
    void* uipi_signal_stack_safe;
    vcontext_t fctx;
    DEFINE_BITMAP(cpu_affinity, NCPU);
    struct uipi_ops uipi_ops;
    atomic_t parked;
    bool to_exit;
    bool to_start;
    bool with_fs;
    int  to_cpu_id;
    uint64_t api_user_stack;
    spinlock_t *miniop_l_ptr;
    atomic64_t *miniop_cnt_ptr;
    uint64_t   *miniop_base_ptr;
    uint64_t   *miniop_last_ptr;
    minimal_ops_t minimal_ops;
    minimal_ops_t minimal_ops_je;
    int cede_cnt;
}__attribute__((aligned(64)));
typedef struct cluster_ctx cluster_ctx_t;

struct cluster_ctx_map {
    cluster_ctx_t* map[NCPU]; // Registered by runtime.
};
typedef struct cluster_ctx_map cluster_ctx_map_t;



extern cluster_ctx_t* alloc_cluster();
extern void dealloc_cluster(cluster_ctx_t* ctx);
// extern int cluster_set_entry(cluster_ctx_t* ctx, func_t entry);

__noreturn void cluster_exit();
void* cluster_spawn(func_t func, void* args, int pref_core_id);

static inline void set_software_entry(cluster_ctx_t* ctx, func_t entry, void* arg, void *stack, void* tls) {
    ctx->fctx.RSP = (unsigned long long) stack;
    ctx->fctx.RDI = (unsigned long long) arg;
    ctx->fctx.RBP = 0;
    ctx->fctx.RIP = (unsigned long long) entry;
    ctx->fctx.FS = (unsigned long long) tls;
    return;
};

extern int cluster_init(void);


static inline bool cluster_to_steal(void) {
    bool ret = ACCESS_ONCE(perc_core_conn->to_steal);
    ACCESS_ONCE(perc_core_conn->to_steal) = false;
    return ret;
};