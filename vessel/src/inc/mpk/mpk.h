#pragma once
#include <stdio.h>
#include <stdint.h>

#include "base/limits.h"

#include "pkeys.h"

/*
 * Debug prints
 */
//#define ERIM_DBG
#define ERIM_DBM(...) sprintf(stderr, __VA_ARGS__)
#define ERIM_ERR(...) sprintf(stderr, __VA_ARGS__)
  

// PKRU when running trusted (access to both domain 0 and 1)
#define ERIM_TRUSTED_PKRU (0x00000000)
#define ERIM_SAFE_PKEY (1)
#define ERIM_TUNNEL_PKEY (2)

#define __wrpkru_trusted(KEY) \
  do { \
  __label__ vessel_trusted; \
  vessel_trusted: \
    asm goto ( \
      "xor %%eax, %%eax\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%edx, %%edx\n\t" \
      ".byte 0x0f,0x01,0xef\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%ebx, %%ebx\n\t" \
      "RDPKRU\n\t" \
      "cmp %%ebx, %%eax\n\t" \
      "jne %l1\n\t" \
      ::"r"(KEY):"rax", "rcx", "rdx", "rbx", "memory": vessel_trusted \
    ); \
  } while(0)


#define __wrpkru_percpu(MEM_PTR) \
  do { \
  __label__ vessel_start; \
  vessel_start: \
    asm goto ( \
      "xor %%rcx, %%rcx\n\t" \
      "rdtscp\n\t" \
      "andq $0xFFF, %%rcx\n\t" \
      "movabsq $0x8d06a000, %%rax\n\t" \
      "mov (%%rax, %%rcx, 4), %%eax\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%edx, %%edx\n\t" \
      ".byte 0x0f,0x01,0xef\n\t" \
      "xor %%rcx, %%rcx\n\t" \
      "rdtscp\n\t" \
      "andq $0xFFF, %%rcx\n\t" \
      "movabsq $0x8d06a000, %%rax\n\t" \
      "mov (%%rax, %%rcx, 4), %%ebx\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "RDPKRU\n\t" \
      "cmp %%ebx, %%eax\n\t" \
      "jne %l1\n\t" \
      ::"r"(MEM_PTR):"rax", "rcx", "rdx", "rbx", "memory": vessel_start \
    ); \
  } while(0)


// Switching between isolated and application
#define erim_switch_to_trusted						\
  do {                                    \
    __wrpkru_trusted(ERIM_TRUSTED_PKRU);	\
  } while(0)

#define erim_switch_to_untrusted(PTR) \
  do {                                \
    __wrpkru_percpu(PTR);             \
  } while(0)

#define key2pkru(key) \
      0xFFFFFFEC^((3<<(2*key)))

static int inline taskid2mpk (int tid) {
    return tid + 2;
};

struct cpu_pkru_map {
  unsigned int map[NCPU];
};
typedef struct cpu_pkru_map cpu_pkru_map_t;

extern cpu_pkru_map_t  *global_cpu_pkru_map;
extern int mpk_init(void);
