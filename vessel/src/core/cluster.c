#include <string.h>
#include "core/cluster.h"

static pid_t id_cnt=1;
spinlock_t id_cnt_l;

void* cluster_id2cluster_ptr[NCLUSTER];

cluster_ctx_t* alloc_cluster() {
    cluster_ctx_t* new_cluster = (cluster_ctx_t*) malloc(sizeof(cluster_ctx_t));
    memset(new_cluster, 0, sizeof(cluster_ctx_t));

    spin_lock(&id_cnt_l);
    if (id_cnt > NCLUSTER) {
        spin_unlock(&id_cnt_l);
        return NULL;
    }
    new_cluster->id = id_cnt;
    cluster_id2cluster_ptr[id_cnt] = new_cluster; 
    ACCESS_ONCE(id_cnt) = id_cnt + 1;
    barrier();
    spin_unlock(&id_cnt_l);
    new_cluster->cede_cnt = 0;
    return new_cluster;
}

void dealloc_cluster(cluster_ctx_t* ctx) {
    free(ctx);
    return;
}

int cluster_init(void) {
    spin_lock_init(&id_cnt_l);
    memset(cluster_id2cluster_ptr, 0, sizeof(cluster_id2cluster_ptr));
    return 0;
}