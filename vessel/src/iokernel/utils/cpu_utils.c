#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <sys/syscall.h>

#include <base/stddef.h>
#include <base/log.h>

#include <base/sysfs.h>
#include <base/bitmap.h>
#include <base/limits.h>

#include "cpu_utils.h"
int cpu_count;
/* the number of NUMA nodes detected */
int numa_count;
struct cpu_info iokernel_cpu_info_tbl[NCPU];
static int cpu_scan_topology(void)
{
	char path[PATH_MAX];
	DEFINE_BITMAP(numa_mask, NNUMA);
	DEFINE_BITMAP(cpu_mask, NCPU);
	uint64_t tmp;
	int i;

	/* How many NUMA nodes? */
	if (sysfs_parse_bitlist("/sys/devices/system/node/online",
			        numa_mask, NNUMA))
		return -EIO;
	bitmap_for_each_set(numa_mask, NNUMA, i) {
		numa_count++;
		if (numa_count <= i) {
			log_err("cpu: can't support non-contiguous NUMA mask.");
			return -EINVAL;
		}
	}

	if (numa_count <= 0 || numa_count > NNUMA) {
		log_err("cpu: detected %d NUMA nodes, unsupported count.",
			numa_count);
		return -EINVAL;
	}
	
	/* How many CPUs? */
	if (sysfs_parse_bitlist("/sys/devices/system/cpu/online",
			        cpu_mask, NCPU))
		return -EIO;
	bitmap_for_each_set(cpu_mask, NCPU, i) {
		cpu_count++;
		if (cpu_count <= i) {
			log_err("cpu: can't support non-contiguous CPU mask.");
			return -EINVAL;
		}
	}

	if (cpu_count <= 0 || cpu_count > NCPU) {
		log_err("cpu: detected %d CPUs, unsupported count.",
			cpu_count);
		return -EINVAL;
	}

	/* Scan the CPU topology. */
	for (i = 0; i < cpu_count; i++) {
		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/physical_package_id", i);
		if (sysfs_parse_val(path, &tmp))
			return -EIO;
		if (tmp > UINT_MAX)
			return -ERANGE;
		iokernel_cpu_info_tbl[i].package = (int)tmp;

		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/core_siblings_list", i);
		if (sysfs_parse_bitlist(path,
			iokernel_cpu_info_tbl[i].core_siblings_mask, cpu_count))
			return -EIO;

		snprintf(path, sizeof(path), SYSFS_CPU_TOPOLOGY_PATH
			 "/thread_siblings_list", i);
		if (sysfs_parse_bitlist(path,
			iokernel_cpu_info_tbl[i].thread_siblings_mask, cpu_count))
			return -EIO;
	}

	return 0;
}


int cpu_utils_init(void)
{
	int ret = cpu_scan_topology();
	if (ret)
		return ret;

	log_info("cpu: detected %d cores, %d nodes", cpu_count, numa_count);
	return 0;
}
