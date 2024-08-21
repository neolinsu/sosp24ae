#include <inttypes.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>

#include <base/log.h>
#include <base/mem.h>
#include <core/mem.h>

#include "defs.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static vessel_mem_meta_t * __vessel_meta_ptr = NULL;
static void * __vessel_data_ptr = NULL;
static void * __vessel_data_huge_ptr = NULL;

int ctl_shm_init(void)
{
    char *vessel_name = getenv("VESSEL_NAME");
	if(vessel_name == NULL)
	{
		log_err("VESSEL_NAME not set");
		return -1;
	}
	char buf[100];
    sprintf(buf, "vessel_mem_%s", vessel_name);
	int __vessel_meta_fd = shm_open(buf, O_CREAT | O_RDWR, VESSEL_META_PRIO);
    if (!__vessel_meta_fd) {
        log_emerg("Fail to open shared meta memory (key: %s), for %s.",
                    VESSEL_DATA_KEY,
                    strerror(errno));
        return errno;
    }
    int ret = ftruncate(__vessel_meta_fd, VESSEL_MEM_META_SIZE);
    if (ret) {
        log_emerg("Fail to fturncate meta memory fd (%d), for %s.",
                    __vessel_meta_fd,
                    strerror(errno));
        return errno;
    }
	__vessel_meta_ptr = (vessel_mem_meta_t*) mmap(
        (void*)VESSEL_MEM_GLOBAL_START, VESSEL_MEM_META_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, __vessel_meta_fd, 0);
    if (MAP_FAILED == __vessel_meta_ptr || __vessel_meta_ptr != (void*) VESSEL_MEM_GLOBAL_START) {
        log_emerg("Fail to mmap meta memory for %s.",
                    strerror(errno));
        return errno;
    }

	__vessel_data_ptr = mem_map_shm(hash_name(vessel_name,"vessel_data!#"), (void*) VESSEL_MEM_DATA_START, VESSEL_MEM_DATA_SIZE, PGSIZE_4KB, false);
    if (MAP_FAILED == __vessel_data_ptr || __vessel_data_ptr != (void*) VESSEL_MEM_DATA_START) {
        log_emerg("Fail to mmap data memory for vessel_data for %s.",
                    strerror(errno));
        return errno;
    }

    __vessel_data_huge_ptr = mem_map_shm(hash_name(vessel_name,"vessel_data_huge!#"), (void*) VESSEL_MEM_DATA_HUGE_START, VESSEL_MEM_DATA_HUGE_SIZE, PGSIZE_2MB, false);
    if (MAP_FAILED == __vessel_data_huge_ptr || __vessel_data_huge_ptr != (void*) VESSEL_MEM_DATA_HUGE_START) {
        log_emerg("Fail to mmap data huge memory for vessel_data for %s.",
                    strerror(errno));
        return errno;
    }
    log_info("VESSEL_MEM_DATA_HUGE_START: %p to %p", (void*)VESSEL_MEM_DATA_HUGE_START, (void*) (VESSEL_MEM_DATA_HUGE_START + VESSEL_MEM_DATA_HUGE_SIZE));
    return 0;
}