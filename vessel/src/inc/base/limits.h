/*
 * limits.h - maximum limits for different resources
 */

#pragma once

#ifndef CONFIG_NR_CPUS
#error "Need to define the CONFIG_NR_CPUS of the current kernel's config."
#endif
#define CPUMASK_NUM CONFIG_NR_CPUS

#ifndef CONFIG_NODES_SHIFT
#error "Need to define the CONFIG_NODES_SHIFT of the current kernel's config."
#endif
#define NODEMASK_NUM (1 << CONFIG_NODES_SHIFT)

#define VESSEL_MAX_NUMA 3

#if VESSEL_MAX_NUMA < NODEMASK_NUM
#undef NODEMASK_NUM
#define NODEMASK_NUM VESSEL_MAX_NUMA
#endif

#define NCPU        CPUMASK_NUM /* max number of cpus */
#define NVTHREAD	512         /* max number of vessel thread */
//#define NPTHREAD	128         /* max number of pthreads of one vessel task */
#define NCLUSTER	128         /* max number of pthreads of one vessel task */
#define NNUMA		NODEMASK_NUM/* max number of numa zones */
#define NSTAT		1024        /* max number of stat counters */
#define NTASK       13          /* max number of vessel tasks */
#define NMEMD       256         /* max number of memory descripters */
