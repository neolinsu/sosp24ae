/*
 * mem.h - memory management
 */

#pragma once

#include <base/types.h>

enum {
	PGSHIFT_4KB = 12lu,
	PGSHIFT_2MB = 21lu,
	PGSHIFT_1GB = 30lu,
};

enum {
	PGSIZE_4KB = (1llu << PGSHIFT_4KB), /* 4096 bytes */
	PGSIZE_2MB = (1llu << PGSHIFT_2MB), /* 2097152 bytes */
	PGSIZE_1GB = (1llu << PGSHIFT_1GB), /* 1073741824 bytes */
};

#define PGMASK_4KB	(PGSIZE_4KB - 1)
#define PGMASK_2MB	(PGSIZE_2MB - 1)
#define PGMASK_1GB	(PGSIZE_1GB - 1)

/* page numbers */
#define PGN_4KB(la)	(((uintptr_t)(la)) >> PGSHIFT_4KB)
#define PGN_2MB(la)	(((uintptr_t)(la)) >> PGSHIFT_2MB)
#define PGN_1GB(la)	(((uintptr_t)(la)) >> PGSHIFT_1GB)

#define PGOFF_4KB(la)	(((uintptr_t)(la)) & PGMASK_4KB)
#define PGOFF_2MB(la)	(((uintptr_t)(la)) & PGMASK_2MB)
#define PGOFF_1GB(la)	(((uintptr_t)(la)) & PGMASK_1GB)

#define PGADDR_4KB(la)	(((uintptr_t)(la)) & ~((uintptr_t)PGMASK_4KB))
#define PGADDR_2MB(la)	(((uintptr_t)(la)) & ~((uintptr_t)PGMASK_2MB))
#define PGADDR_1GB(la)	(((uintptr_t)(la)) & ~((uintptr_t)PGMASK_1GB))

typedef unsigned long physaddr_t; /* physical addresses */
typedef unsigned long virtaddr_t; /* virtual addresses */

#ifndef MAP_FAILED
#define MAP_FAILED	((void *)-1)
#endif

typedef unsigned int mem_key_t;

extern void *mem_map_shm(mem_key_t key, void *base, size_t len,
			 size_t pgsize, bool exclusive);
extern void *mem_map_shm_rdonly(mem_key_t key, void *base, size_t len,
			 size_t pgsize);
extern int mem_unmap_shm(void *base);
extern int mem_lookup_page_phys_addrs(void *addr, size_t len, size_t pgsize,
				      physaddr_t *maddrs);
extern long mbind(void *start, size_t len, int mode,
	   const unsigned long *nmask, unsigned long maxnode,
	   unsigned flags);
/**
 * May issue error signal. 
*/
extern void touch_mapping(void *base, size_t len, size_t pgsize);
static inline int
mem_lookup_page_phys_addr(void *addr, size_t pgsize, physaddr_t *paddr)
{
	return mem_lookup_page_phys_addrs(addr, pgsize, pgsize, paddr);
}

extern void *mem_map_file(void *base, size_t len, int fd, off_t offset);

static inline int hash_name(const char* name,const char* key) {
    int hash = 0;
    while (*name) {
        hash = hash * 33 + *name++;
    }
    while (*key) {
        hash = hash * 33 + *key++;
    }
    return hash;
}