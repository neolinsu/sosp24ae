#pragma once
#include <stddef.h>

#include <base/cpu.h>
#include <base/tcb.h>
#include <base/bitmap.h>
#include <vcontext/vcontext.h>
typedef void(*vessel_func_inf_t)(void*);
#ifndef VESSEL_ORI 
////////////////////////////////////////////////////
// Sync with cluster.h
struct fctx_map {
    volatile vcontext_t map[NCPU]; // Registered by runtime.
};
typedef struct fctx_map fctx_map_t;
////////////////////////////////////////////////////
extern const fctx_map_t *fctx_map;
#endif
/// @brief From src/inc/core/uipi.h
typedef void(*op_func_inf_t)(void);

struct uipi_ops_inf {
   op_func_inf_t  to_cede;
   op_func_inf_t  to_yield;
};
// End of src/inc/core/uipi.h

// Memory
extern void* vessel_alloc_chunk(size_t chunk_num);
extern void *vessel_alloc_huge_chunk(size_t chunk_num);
// CLuster
extern void* vessel_cluster_spawn(vessel_func_inf_t func, void* args, int pref_core_id);
extern int vessel_cluster_spawn_all(vessel_func_inf_t func, void* args);
extern void vessel_cluster_yield(void);
extern void vessel_cluster_exit(void);

// IPI
extern int vessel_register_uipi_handlers(struct uipi_ops_inf *ops, int* uipi_vector_cede_fd_p, int* uipi_vector_yield_fd_p);


// TCB
extern tcbhead_t* vessel_create_tcb(void);
extern tcbhead_t* vessel_init_tcb(tcbhead_t* allocated_tcb);
extern int vessel_dealloc_tcb(tcbhead_t* allocated_tcb, bool dealloc_tcb);

extern cpu_info_t * vessel_get_cpu_info(void);

extern void vessel_cluster_mwait(void);
extern int vessel_cluster_id(void);
extern int vessel_cluster_to_steal(void);