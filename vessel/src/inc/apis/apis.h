#include <stdint.h>
#include <base/bitmap.h>
#include <base/tcb.h>
#include <base/cpu.h>
#include <core/uipi.h>


struct vessel_alloc_chunk_api_args {
    size_t chunk_num_i;
    void*  allocated_o;
};

struct vessel_alloc_huge_chunk_api_args {
    size_t chunk_num_i;
    void*  allocated_o;
};

typedef void(*apis_func_t)(void*);
struct vessel_cluster_spawn_api_args {
    apis_func_t func_i;
    void*  args_i;
    int pref_core_id_i;
    void* ret_o;
};

struct vessel_spawn_all_clusters_api_args {
    apis_func_t func_i;
    void*  args_i;
    int ret_o;
};

struct vessel_register_uipi_handlers_api_args {
    struct uipi_ops *ops_i;
    int uipi_vector_cede_fd;
    int uipi_vector_yield_fd;
    int ret_o;
};

struct vessel_create_tcb_api_args {
    tcbhead_t *ret_o;
};

struct vessel_init_tcb_api_args {
    tcbhead_t *tcb_i;
    tcbhead_t *ret_o;
};

struct vessel_dealloc_tcb_api_args {
    tcbhead_t *tcb_i;
    bool dealloc_tcb_i;
    int ret_o;
};

struct vessel_get_cpu_info_args {
    cpu_info_t *ret_o;
};

struct vessel_cluster_id_args {
    int ret_o;
};

struct vessel_cluster_to_steal_args {
    int ret_o;
};

typedef void(*api_op_func_t)(void*);
struct vessel_apis {
    api_op_func_t alloc_chunk;
    api_op_func_t alloc_huge_chunk;
    api_op_func_t cluster_spawn;
    api_op_func_t cluster_spawn_all;
    api_op_func_t cluster_yield;
    api_op_func_t cluster_exit;
    api_op_func_t create_tcb;
    api_op_func_t init_tcb;
    api_op_func_t dealloc_tcb;
    api_op_func_t register_uipi_handlers;
    api_op_func_t get_cpu_info;
    api_op_func_t cluster_mwait;
    api_op_func_t cluster_id;
    api_op_func_t cluster_to_steal;
};
typedef struct vessel_apis vessel_apis_t;

int apis_init(void);
