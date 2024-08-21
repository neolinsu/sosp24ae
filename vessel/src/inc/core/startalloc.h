#pragma once
#include <stddef.h>
#include <stdint.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/lock.h>

#include <core/cluster.h>
#include <core/kthread.h>


#define START_MEM_CHUNK_NUM 1llu
#define START_MEM_END       START_MEM_CHUNK_NUM * VESSEL_MEM_BASIC_CHUNK_SIZE
void *
start_malloc (size_t n);


void *
start_aligned_alloc (size_t align, size_t len);

void *
start_calloc (size_t num, size_t size);

void *
start_realloc (void *ptr, size_t size);

/* This will rarely be called.  */
void
start_free (void *ptr);