#pragma once

#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>

#include "base/list.h"
#include "base/lock.h"
#include "base/bitmap.h"
#include "base/limits.h"


/*
 * Memory map of vessel.
 * ------------------ VESSEL_MEM_GLOBAL_START
 * | Meta-data of vessel
 * ------------------ VESSEL_MEM_GLOBAL_START + VESSEL_MEM_META_SIZE_GB
 * |                        |
 * | A chunk for one vessel | Managed by jemalloc.
 * |                        |
 * ------------------ VESSEL_MEM_GLOBAL_START + VESSEL_MEM_META_SIZE + requested_chunk_size
 * |
 * | ...
 * |
 * ------------------ VESSEL_MEM_GLOBAL_START + VESSEL_MEM_GLOBAL_SIZE
 */
// META
#define VESSEL_META_KEY	        "vessel_meta"
#define VESSEL_DATA_KEY	        "vessel_data"
#define VESSEL_DATA_HUGE_KEY    "vessel_data_huge"
#define VESSEL_META_PRIO	0666 // TODO: make it root only	
#define VESSEL_DATA_PRIO	0666 // TODO: make it root only	
#define VESSEL_MAGIC_CODE 0xEB0832FAll
#define VESSEL_MEM_BASIC_CHUNK_SIZE	1 * GB
#define VESSEL_CHUNK_NUM VESSEL_MEM_DATA_SIZE_GB
#define VESSEL_HUGE_CHUNK_NUM VESSEL_MEM_DATA_SIZE_GB

// Memory Map
#define VESSEL_MEM_GLOBAL_START_GB      2llu  // 2GB
#define VESSEL_MEM_META_SIZE_GB         1llu  //+1GB
#define VESSEL_MEM_DATA_SIZE_GB        64llu  //+64GB
#define VESSEL_MEM_DATA_HUGE_SIZE_GB   16llu  //+8GB

#define VESSEL_MEM_DATA_START_GB	(VESSEL_MEM_GLOBAL_START_GB + VESSEL_MEM_META_SIZE_GB)  //+16GB
#define VESSEL_MEM_DATA_HUGE_START_GB	(VESSEL_MEM_DATA_START_GB + VESSEL_MEM_DATA_SIZE_GB)  //+16GB

#define VESSEL_MEM_GLOBAL_START	(VESSEL_MEM_GLOBAL_START_GB * GB)	//	2GB
#define VESSEL_MEM_DATA_START	(VESSEL_MEM_DATA_START_GB   * GB)	//	2GB
#define VESSEL_MEM_DATA_HUGE_START	(VESSEL_MEM_DATA_HUGE_START_GB   * GB)	//	2GB
#define VESSEL_MEM_META_SIZE    (VESSEL_MEM_META_SIZE_GB	* GB)   // +1GB
#define VESSEL_MEM_DATA_SIZE	(VESSEL_MEM_DATA_SIZE_GB	* GB)   //+16GB
#define VESSEL_MEM_DATA_HUGE_SIZE	(VESSEL_MEM_DATA_HUGE_SIZE_GB	* GB)   //+16GB

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define ALIGN_SIZE  4096
#define UPPER_ALIGN(size)   (((size) + ALIGN_SIZE - 1) & (~(ALIGN_SIZE - 1)))
#define PADSIZE(size) UPPER_ALIGN(size) - (size)

// Memory Descriptor
struct vessel_md_meta {
    int tid; // vessel task id
    uint64_t chunk_num;
    uint64_t chunk_start;
    struct list_node link;
};

struct vessel_mem_meta {
    uint64_t _magic; // MAGIC CODE 0xEB 08 32 FA
    spinlock_t mem_lock;
    DEFINE_BITMAP(chunk_alloc, VESSEL_CHUNK_NUM);
    DEFINE_BITMAP(huge_chunk_alloc, VESSEL_HUGE_CHUNK_NUM);
    struct vessel_md_meta md_list[NMEMD];
    struct vessel_md_meta huge_md_list[NMEMD];
    int32_t md_anchor;
    int32_t huge_md_anchor;
};
typedef struct vessel_mem_meta vessel_mem_meta_t;


struct vessel_pt_flush_args {
    void* start;
    size_t size;
};
typedef struct vessel_pt_flush_args vessel_pt_flush_args_t;

struct vessel_pt_flush_api {
    vessel_pt_flush_args_t in;
    vessel_pt_flush_args_t out;
};
typedef struct vessel_pt_flush_api vessel_pt_flush_api_t;

struct vessel_pt_flush_map {
    vessel_pt_flush_api_t in;
    vessel_pt_flush_api_t out;
};

typedef void*(*aligned_alloc_func_t)(size_t, size_t);
typedef void*(*malloc_func_t)(size_t);
typedef void*(*realloc_func_t)(void*, size_t);
typedef void*(*calloc_func_t)(size_t, size_t);
typedef void(*free_func_t)(void*);

struct minimal_ops {
    void* aligned_alloc;
    void* malloc;
    void* calloc;
    void* free;
    void* realloc;
};
typedef struct minimal_ops minimal_ops_t;

struct minimal_ops_map {
    minimal_ops_t *map[NCPU];
};
typedef struct minimal_ops_map minimal_ops_map_t;


/**
 * init_mem - ...
 * Returns 0 successful, otherwise failure.
 */
extern int mem_init(int force);

/**
 * exit_mem - ...
 * Returns 0 successful, otherwise failure.
 */
extern int exit_mem(int tid, struct list_head *head);

/**
 * clear_mem - ...
 * Returns 0 successful, otherwise failure.
 */
extern int clear_mem();

/**
 * alloc_chunk - ...
 * Returns fd > 0. Return 0 for failure.
 */
extern void* alloc_chunk(int tid, size_t chunk_num, struct list_head *head);

/**
 * alloc_chunk - ...
 * Returns fd > 0. Return 0 for failure.
 */
extern void* alloc_huge_chunk(int tid, size_t chunk_num, struct list_head *head);

/**
 * free_chunk - ...
 * Returns 0 successful, otherwise failure.
 */
extern int free_chunk(int tid, int md, struct list_head *head);

