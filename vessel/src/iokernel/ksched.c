/*
 * ksched.c - an interface to the ksched kernel module
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <base/log.h>
#include <base/bitmap.h>
#include <base/mem.h>
#include <base/stddef.h>

#include "scheds/state.h"
#include "ksched.h"

/* a file descriptor handle to the ksched kernel module */
int ksched_fd;
/* the number of pending interrupts */
int ksched_count;
/* the shared memory region with the kernel module */
struct ksched_shm_cpu *ksched_shm;
/* the set of pending cores to send interrupts to */
cpu_set_t ksched_set;
/* the generation number for each core */
unsigned int ksched_gens[NCPU];

int yield_op[NCPU];
int cede_op[NCPU];
int yield_fd[NCPU];
int cede_fd[NCPU];

struct core_conn_map * cs_map;

DEFINE_BITMAP(cpu_tosend, NCPU);
/**
 * ksched_init - initializes the ksched kernel module interface
 *
 * Returns 0 if successful.
 */
int ksched_init(void)
{
	log_debug("ksched init start.");
	memset(cpu_msr_fd, 0, sizeof(cpu_msr_fd));
	memset(cpu_last_sel, 0, sizeof(cpu_last_sel));
	memset(cpu_val, 0, sizeof(cpu_val));
	memset(cpu_tsc, 0, sizeof(cpu_tsc));
	memset(ksched_gens, 0, sizeof(ksched_gens));
	memset(yield_op, -1, sizeof(yield_op));
	memset(cede_op, -1, sizeof(cede_op));
	mem_key_t shm_id;
	if(strcmp("server", getenv("VESSEL_NAME")) == 0) {
		shm_id = VESSEL_CORE_STATE_ID_SERVER;
	} else if (strcmp("client", getenv("VESSEL_NAME")) == 0) {
		shm_id = VESSEL_CORE_STATE_ID_CLIENT;
	} else {
		log_err("name err");
		abort();
	}

	cs_map = mem_map_shm(shm_id, (void*)VESSEL_CORE_STATE_BASE, VESSEL_CORE_STATE_SIZE, PGSIZE_2MB, false);
	BUG_ON(cs_map==MAP_FAILED || cs_map==NULL);
	#ifdef VESSEL_UIPI
	memset(cs_map, 0, sizeof(*cs_map));
	for (int i=0; i<NCPU; i++) {
		ACCESS_ONCE(cs_map->map[i].pmc_done) = true;
	}
	#endif
	log_debug("ksched init done.");
    return 0;
}