/*
 * cpu.c - support for scanning cpu topology.
 */
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <elf.h>
#include <sys/syscall.h>
#include <sys/auxv.h>

#include "base/stddef.h"
#include "base/log.h"
#include "base/cpu.h"
#include "base/sysfs.h"
#include "base/bitmap.h"
#include "base/limits.h"

#include "core/init.h"

extern cpu_state_map_t *global_cpu_state_map;
/* a table of information on each CPU */
cpu_info_t cpu_info_tbl;

#ifndef HWCAP2_FSGSBASE
#define HWCAP2_FSGSBASE        (1 << 1)
#endif

static int check_fsgsbase(){
    unsigned val = getauxval(AT_HWCAP2);
    if (val & HWCAP2_FSGSBASE) return true;
    return false;
}

int init_process_cpuset(cpu_info_t *cpu_info, bitmap_ptr_t affinitive_cpus) {
    char buf[BUFSIZ]     = {0};
    char listbuf[BUFSIZ] = {0};
    char path[PATH_MAX] = {0};
    DEFINE_BITMAP(tmp_cpumask, NCPU);
    int ret=0;
    uint64_t tmp;
    int index;

    bitmap_init(cpu_info->cpu_mask, NCPU, 0);
    bitmap_init(cpu_info->numa_mask, NNUMA, 0);

	if (!affinitive_cpus) {
		FILE *fp = fopen("/proc/self/status", "r");

		if(fp == NULL) {
			log_err("Fail to open /proc/self/status, errno=%d", errno);
			return errno; 
		}
		while (fgets(buf, sizeof(buf), fp) != NULL)
		{
			ret = sscanf(buf, "Cpus_allowed_list: %s\n", listbuf);
			if (ret == 1) {
				ret = string_to_bitmap(listbuf, tmp_cpumask, NCPU);
				if (!!ret) {
					log_err("Fail to parse %s.", listbuf);
					return -EINVAL;
				}
				bitmap_or(cpu_info->cpu_mask, cpu_info->cpu_mask, tmp_cpumask, NCPU);
			}
		}
	}
	else {
		bitmap_for_each_set(affinitive_cpus, NCPU, index) {
			bitmap_set(cpu_info->cpu_mask, index);
		}
	}
	DEFINE_BITMAP(tmp_bitmap, NCPU);
	bitmap_init(tmp_bitmap, NCPU, false);
	bitmap_xor(tmp_bitmap, cpu_info_tbl.cpu_mask, cpu_info->cpu_mask, NCPU);
	bitmap_and(tmp_bitmap, cpu_info->cpu_mask, tmp_bitmap, NCPU);
	ret = bitmap_popcount(tmp_bitmap, NCPU);
	if (ret > 0) {
		log_err("affinitive_cpus is out of scope.");
		return EINVAL;
	}
	ret = bitmap_popcount(cpu_info->cpu_mask, NCPU);
	if (ret == 0) {
		log_err("affinitive_cpus is empty, or no ava cpu.");
        return EINVAL;
    }
	cpu_info->cpu_count = ret;
	
    bitmap_for_each_set(cpu_info->cpu_mask, NCPU, index) {
        snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/physical_package_id", index);
        if (sysfs_parse_val(path, &tmp))
			return EIO;
        if (tmp > UINT_MAX)
			return ERANGE;
        bitmap_set(cpu_info->numa_mask, tmp);
    }
	ret = bitmap_popcount(cpu_info->numa_mask, NNUMA);
	if (ret == 0) {
        return EINVAL;
    }
	cpu_info->numa_count = ret;
    return 0;
}

static int cpu_scan_topology(cpu_info_t *cpu_info)
{
	char path[PATH_MAX];
	uint64_t tmp;
	int i, ret = 0;
	DEFINE_BITMAP(tmp_mask, NCPU);
	bitmap_for_each_set (cpu_info->cpu_mask, NCPU, i) {
		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/physical_package_id", i);
		if (sysfs_parse_val(path, &tmp))
			return EIO;
		if (tmp > UINT_MAX)
			return ERANGE;
		
		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/core_siblings_list", i);
		ret = sysfs_parse_bitlist(path,
			tmp_mask, NCPU);
		if (ret)
			return EIO;
		
		bitmap_init(cpu_info->map[i].core_siblings_mask, NCPU, false);
		bitmap_and(cpu_info->map[i].core_siblings_mask, tmp_mask, cpu_info->cpu_mask, NCPU);
	
		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/thread_siblings_list", i);
		
		ret = sysfs_parse_bitlist(path,
			tmp_mask, NCPU);
		if (ret)
			return EIO;
		bitmap_init(cpu_info->map[i].thread_siblings_mask, NCPU, false);
		bitmap_and(cpu_info->map[i].thread_siblings_mask, tmp_mask, cpu_info->cpu_mask, NCPU);
	}

	return 0;
}

static int take_cpu(cpu_info_t *cpu_info) {
	int i, ret;
	spin_lock(&(global_cpu_state_map->lock));
	bitmap_for_each_set (cpu_info->cpu_mask, cpu_info->cpu_count, i) {
		if (bitmap_test(global_cpu_state_map->state, i)) {
			ret = EAGAIN;
			log_crit("Failed to allocate cpu:%d, that has been taken.\n", i);
			goto out;
		}
	}
	bitmap_for_each_set (cpu_info->cpu_mask, cpu_info->cpu_count, i) {
		bitmap_set(global_cpu_state_map->state, i);
	}
	ret = 0;
out:
	spin_unlock(&(global_cpu_state_map->lock));
	return ret;
}

// static int cpu_dealloc(cpu_info_t *cpu_info) {
// 	int ret;
// 	spin_lock(&(global_cpu_state_map->lock));
// 	ret = 0;
// 	spin_unlock(&(global_cpu_state_map->lock));
// 	return ret;
// }

/**
 * cpu_alloc - initializes CPU support for one task.
 *
 * Returns zero if successful, otherwise fail.
 */
int cpu_alloc(cpu_info_t *cpu_info, bitmap_ptr_t affinitive_cpus, int exclusive)
{
	int i, ret = 0;

	ret = init_process_cpuset(cpu_info, affinitive_cpus);
	if (ret) return ret;

	ret = cpu_scan_topology(cpu_info);
	if (ret) return ret;

	if (exclusive) {
		ret = take_cpu(cpu_info);
		if (ret) return ret;
	}
	uint32_t cur_cpu_id = get_cur_cpuid();
	if (!bitmap_test(cpu_info->cpu_mask, cur_cpu_id)) {
		i = bitmap_find_next_set(cpu_info->cpu_mask, NCPU, 0);
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(i, &cpuset);
		ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
		if (ret) {
			log_err("Fail to set task's cpu affinity to within affinitive_cpus for %s.", strerror(ret));
			return ret;
		}
	}
	
	log_info("cpu: detected %d cores, %d nodes", cpu_info->cpu_count, cpu_info->numa_count);
	return 0;
}


/**
 * cpu_init - initializes CPU support for cpuinfo.
 *
 * Returns zero if successful, otherwise fail.
 */
int cpu_init()
{
	int ret = 0;
	ret = check_fsgsbase();
	if (ret==false) {
		log_err("OS does not support fsgsbase.");
		// return ret; 
	}

	ret = init_process_cpuset(&cpu_info_tbl, NULL);
	if (ret) return ret;

	ret = cpu_scan_topology(&cpu_info_tbl);
	if (ret) return ret;

	log_info("cpu: detected %d cores, %d nodes", cpu_info_tbl.cpu_count, cpu_info_tbl.numa_count);
	return 0;
}
