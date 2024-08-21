#include <sys/syscall.h>
#include <unistd.h>
#include <x86gprintrin.h>
#include <string.h>
#include <errno.h>

#include "base/log.h"
#include "base/compiler.h"
#include "base/cpu.h"
#include "base/mem.h"
#include "core/task.h"
#include "core/kthread.h"
#include "core/uipi.h"
#include "core/pmc.h"
#include "vessel/interface.h"
#include "vcontext/vcontext.h"
#include "mpk/mpk.h"
#include <scheds/state.h>


#ifndef __x86_64__
#error Support x86_64 only!
#endif

#ifndef __NR_uintr_register_handler
#define __NR_uintr_register_handler	  450
#define __NR_uintr_unregister_handler 451
#define __NR_uintr_create_fd          452
#define __NR_uintr_register_sender    453
#define __NR_uintr_unregister_sender  454
#define __NR_uintr_wait               455
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(fd, flags)	syscall(__NR_uintr_unregister_sender, fd, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)

extern cluster_ctx_map_t  *global_cluster_ctx_map;
extern kthread_fs_map_t  *global_kthread_fs_map;
extern fctx_map_t *global_fctx_map;

extern void uipi_entry(void *);
extern void switch_to(void *);

struct core_conn_map * cs_map;
__thread struct core_conn *perc_core_conn;
// This function should be linked into mpk.channel,
// while the rest should be on the mpk.safe. 

void uipi_handler(vcontext_t *v_frame) {
    #ifdef VESSEL_UIPI
    // Don't use __thread to avoid fs thrash.
    erim_switch_to_trusted;
    uint32_t cpuid = get_cur_cpuid();
    uint64_t fs = v_frame->FS;
    if (unlikely(fs==global_kthread_fs_map->kthread_fs[cpuid]) ||
        (cs_map->map[cpuid].gen == cs_map->map[cpuid].last_gen) ) { 
        // Return if the current context is in vessel.
        switch_to((void*)v_frame);
        log_err("Wrong branch!");
        abort();
    }

    cluster_ctx_t *cur = global_cluster_ctx_map->map[cpuid];
    if (cur->fctx.FS != fs) {
        log_err("uipi_handler -> global_cluster_ctx_map: data race");
        abort();
    }
    cur->cede_cnt++;
    struct uipi_ops *cur_ops = &(cur->uipi_ops);
    barrier();
    //if(likely(v_frame->vector == UIPI_VECTOR_CEDE)) {
    if(likely(cs_map->map[cpuid].gen != cs_map->map[cpuid].last_gen)) {
        erim_switch_to_untrusted(0);
        cur_ops->to_cede();
        erim_switch_to_trusted;
        cur->cede_cnt--;
        switch_to((void*)v_frame);
        log_err("Wrong branch!");
        abort();
    }
    switch(v_frame->vector) {
        case UIPI_VECTOR_YIELD:
            log_err("UIPI_VECTOR_YIELD");
            abort();
            erim_switch_to_untrusted(0);
            cur_ops->to_yield();
            erim_switch_to_trusted;
            cur->cede_cnt--;
            switch_to((void*)v_frame);
            log_err("Wrong branch!");
            abort();
        break;
        default:
        log_err("Unknown Area!");
        abort();
    }
    log_err("Unknown Area!");
    abort();
    #endif // VESSEL_UIPI
}


__thread int my_uipi_vector_cede_fd = 0;
__thread int my_uipi_vector_yield_fd = 0;


// Call while init kthread
int register_uipi_handlers (void) {
    int ret=0;
#ifdef VESSEL_UIPI
    ret = uintr_register_handler(uipi_entry, 0);
    if (ret) {
        log_err("Fail to register uipi_entry for %d.", ret);
        return ret;
    }
    my_uipi_vector_cede_fd = uintr_create_fd(UIPI_VECTOR_CEDE, 0);
    if (my_uipi_vector_cede_fd<0) {
        log_err("Fail to uintr_create_fd for %s.", strerror(errno));
        return errno;
    }
    log_debug("UIPI my_uipi_vector_cede_fd: %d", my_uipi_vector_cede_fd);

    my_uipi_vector_yield_fd = uintr_create_fd(UIPI_VECTOR_YIELD, 0);
    if (my_uipi_vector_yield_fd<0) {
        log_err("Fail to uintr_create_fd for %s.", strerror(errno));
        return errno;
    }
    log_debug("UIPI my_uipi_vector_yield_fd: %d", my_uipi_vector_yield_fd);
    
    //log_info("yield %p, %d", &(cs_map->map[get_cur_cpuid()].yield_fd), cs_map->map[get_cur_cpuid()].yield_fd);
    
#else

#endif // VESSEL_UIPI
    perc_core_conn = &(cs_map->map[get_cur_cpuid()]);
    // TODO: Add more ops.
    return ret;
}
