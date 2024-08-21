/*
 * init.c - support for initialization
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "base/log.h"
#include "base/cpu.h"
#include "base/time.h"
#include "base/bitmap.h"
#include "base/mem.h"

#include "core/mem.h"
#include "core/task.h"
#include "core/cluster.h"
#include "core/kthread.h"

#include "core/config.h"
#include "core/init.h"
#include "core/meta.h"

#include "scheds/state.h"
#include "mpk/mpk.h"
#include "apis/apis.h"
bool core_init_done=false;

void __weak init_shutdown(int status)
{
	log_info("init: shutting down -> %s",
		 status == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
	exit(status);
}

int force_init_memory_meta=0;
int force_init_vessel_meta=0;

extern register_tls_ops_func_t *global_register_tls_ops_func;
extern struct core_conn_map * cs_map;

/**
 * task_init - initializes before loading task and its main vcluster.
 *
 * Returns 0 if successful, otherwise fail.
 */
int task_init(bitmap_ptr_t affinitive_cpus, int exclusive,
				char* path, int argc, char** argv, char** env, uid_t uid) {
	int ret = 0;
	task_t *new_task;

	new_task = task_alloc();
	if (unlikely(!new_task)) {
		log_emerg("Fail to alloc task, for %s", strerror(errno));
		return errno;
	}
	ret = cpu_alloc(&(new_task->cpu_info), affinitive_cpus, exclusive); // exclusive only affact among each other.
	if (unlikely(ret)) {
		log_emerg("Fail to alloc cpu, for %s.", strerror(ret));
		return ret;
	}
	
	
	ret = kthread_alloc(new_task);
	if (unlikely(ret)) {
		log_emerg("Fail to alloc kthread, for %s.", strerror(ret));
		return ret;
	}
	
	task_start_args_t *sargs = malloc(sizeof(task_start_args_t));
	sargs->path = path;
	sargs->argc = argc;
	sargs->argv = argv;
	sargs->env  = env ;
	sargs->uid  = uid ;
	cluster_ctx_t *main_cluster = build_task_start_cluster(new_task, sargs);
	ret = task_attach_cluster(new_task, main_cluster);
	if (unlikely(ret)) {
		log_emerg("Fail to attach main cluster to task, for %s.", strerror(ret));
		return ret;
	}
	int cpu;
	bitmap_for_each_set(new_task->cpu_info.cpu_mask, NCPU, cpu) {
		log_debug("attach main to %d", cpu);
again:
		ret = kthread_attch_cluster(&(global_kthread_meta_map->kthread_meta[cpu]),
			main_cluster);
		if (ret) {
			goto again;
		}
		break;
	}
	ret = 0;
	return ret;
}

/**
 * core_init - initializes before loading each vessel task.
 *
 * Returns 0 if successful, otherwise fail.
 */
int core_init(void)
{
	int ret;
	ret = config_init();
	if (ret) return ret;
	log_debug("start mem_init");
	ret = mem_init(force_init_memory_meta);
	if (ret) return ret;

	ret = mpk_init();
	if (ret) return ret;

	ret = cpu_init();
	if (ret) return ret;

	log_debug("start meta_init");
	ret = meta_init();
	if (ret) return ret;

	if (force_init_vessel_meta) {
		log_debug("start rebuild_meta");
		ret = rebuild_meta();
		if (ret) return ret;
	}

	ret = time_init();
	if (ret) return ret;
	mem_key_t shm_id;
	if(strcmp("server", getenv("VESSEL_NAME")) == 0) {
		shm_id = VESSEL_CORE_STATE_ID_SERVER;
	} else if (strcmp("client", getenv("VESSEL_NAME")) == 0) {
		shm_id = VESSEL_CORE_STATE_ID_CLIENT;
	} else {
		log_err("name err");
		abort();
	}
    log_info("base:%p, size:%lu", (void*)VESSEL_CORE_STATE_BASE, VESSEL_CORE_STATE_SIZE);
    cs_map = mem_map_shm(shm_id, (void*)VESSEL_CORE_STATE_BASE, VESSEL_CORE_STATE_SIZE, PGSIZE_2MB, false);
	BUG_ON(cs_map==MAP_FAILED || cs_map==NULL);

	if (force_init_vessel_meta) {
		ret = cluster_init();
		if (ret) return ret;
		ret = task_meta_init();
		if (ret) return ret;
		ret = kthread_meta_init();
		if (ret) return ret;
		ret = apis_init();
		if (ret) return ret;
#ifdef VESSEL_UIPI
		memset(cs_map, 0, sizeof(*cs_map));
#endif
		*global_register_tls_ops_func = startup_vessel_register_tls_ops;
	}
	core_init_done = true;
	log_init_done = true;
	return 0;
}

// TODO
// kthread_alloc();