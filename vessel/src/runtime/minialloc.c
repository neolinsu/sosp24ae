#include <stddef.h>
#include <stdint.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/lock.h>
#include <vessel/interface.h>

#include <runtime/minialloc.h>

#include "defs.h"

#define MINI_ALLOC_CHUNK_2MB 4llu // 4GB
#define MINI_ALLOC_CHUNK_4K  2llu // 2GB
#define MINI_ALLOC_CHUNK_C   1llu // 1GB
static void *mini_alloc_ptr=NULL, *mini_alloc_end=NULL, *mini_alloc_last_block=NULL;
static void *mini_data_ptr=NULL;

static void *mini_alloc_4K_ptr=NULL, *mini_alloc_4K_end=NULL, *mini_alloc_last_block_4K=NULL;
static void *mini_data_4K_ptr=NULL;

static void *mini_alloc_C_ptr=NULL, *mini_alloc_C_end=NULL, *mini_alloc_last_block_C=NULL;
static void *mini_data_C_ptr=NULL;

DEFINE_SPINLOCK(mini_alloc_lock);
DEFINE_SPINLOCK(mini_alloc_4K_lock);
DEFINE_SPINLOCK(mini_alloc_C_lock);

void *
mini_malloc (size_t n)
{
  spin_lock(&mini_alloc_lock);
  barrier();
  /* Make sure the allocation pointer is ideally aligned.  */
  mini_alloc_ptr = (void*)align_up((uint64_t)mini_alloc_ptr, PGSIZE_2MB);
  if (mini_alloc_ptr + n >= mini_alloc_end || n >= -(uintptr_t) mini_alloc_ptr)
    {
      log_err("mini_malloc: out of bound.");
      abort();
    }
  assert(n%PGSIZE_2MB == 0);
  mini_alloc_last_block = mini_alloc_ptr;
  ACCESS_ONCE(mini_alloc_ptr) = mini_alloc_ptr + n;
  barrier();
  spin_unlock(&mini_alloc_lock);
  // log_info("mini_alloc_last_block: %p", mini_alloc_last_block);
  return mini_alloc_last_block;
}

void *
mini_malloc_C (size_t n)
{
  spin_lock(&mini_alloc_C_lock);
  barrier();
  /* Make sure the allocation pointer is ideally aligned.  */
  mini_alloc_C_ptr = (void*)align_up((uint64_t)mini_alloc_C_ptr, CACHE_LINE_SIZE);
  if (mini_alloc_C_ptr + n >= mini_alloc_C_end || n >= -(uintptr_t) mini_alloc_C_ptr)
    {
      log_err("mini_malloc: out of bound.");
      abort();
    }
  assert(n%CACHE_LINE_SIZE == 0);
  mini_alloc_last_block_C = mini_alloc_C_ptr;
  ACCESS_ONCE(mini_alloc_C_ptr) = mini_alloc_C_ptr + n;
  barrier();
  spin_unlock(&mini_alloc_C_lock);
  return mini_alloc_last_block_C;
}


void *
mini_malloc_4K (size_t n)
{
  spin_lock(&mini_alloc_4K_lock);
  barrier();
  /* Make sure the allocation pointer is ideally aligned.  */
  mini_alloc_4K_ptr = (void*)align_up((uint64_t)mini_alloc_4K_ptr, PGSIZE_4KB);
  if (mini_alloc_4K_ptr + n >= mini_alloc_4K_end || n >= -(uintptr_t) mini_alloc_4K_ptr)
    {
      log_err("mini_malloc_4K: out of bound.");
      abort();
    }
  assert(n%PGSIZE_4KB == 0);
  mini_alloc_last_block_4K = mini_alloc_4K_ptr;
  ACCESS_ONCE(mini_alloc_4K_ptr) = mini_alloc_4K_ptr + n;
  barrier();
  spin_unlock(&mini_alloc_4K_lock);
  // log_info("mini_alloc_last_block_4K: %p", mini_alloc_last_block_4K);
  return mini_alloc_last_block_4K;
}


/* This will rarely be called.  */
void
mini_free (void *ptr)
{
  log_err("Touch mini_free!");
  abort();
}

int minialloc_init(void) {
  spin_lock_init(&mini_alloc_lock);
  spin_lock_init(&mini_alloc_4K_lock);
  spin_lock_init(&mini_alloc_C_lock);

  mini_alloc_ptr = mini_data_ptr = vessel_alloc_huge_chunk(MINI_ALLOC_CHUNK_2MB);
  mini_alloc_4K_ptr = mini_data_4K_ptr = vessel_alloc_chunk(MINI_ALLOC_CHUNK_4K);
  mini_alloc_C_ptr = mini_data_C_ptr = vessel_alloc_chunk(MINI_ALLOC_CHUNK_C);

  if(mini_alloc_ptr==NULL) {
    log_err("No mem.");
    return ENOMEM;
  }
  if(mini_alloc_4K_ptr==NULL) {
    log_err("No mem.");
    return ENOMEM;
  }
  if(mini_alloc_C_ptr==NULL) {
    log_err("No mem.");
    return ENOMEM;
  }

  mini_alloc_end = mini_alloc_ptr + MINI_ALLOC_CHUNK_2MB*GB;
  mini_alloc_4K_end = mini_alloc_4K_ptr + MINI_ALLOC_CHUNK_4K*GB;
  mini_alloc_C_end = mini_alloc_C_ptr + MINI_ALLOC_CHUNK_C*GB;
  return 0;
}