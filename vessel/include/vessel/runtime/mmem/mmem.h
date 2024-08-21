#pragma once
#include <numaif.h>

#include <base/stddef.h>
#include <base/cpu.h>
#include <base/log.h>
#include <base/bitmap.h>
#include <base/mem.h>

#include <runtime/minialloc.h>

static inline int bind_mem_on_node(void* addr, size_t len, int node) {
    int ret = 0;
     unsigned long mask = (1 << node);
     ret = mbind(addr, len, MPOL_BIND, &mask, 64, MPOL_MF_STRICT | MPOL_MF_MOVE);
     if (unlikely(ret)) {
         log_err("Fail to ve_aligned_alloc_on_node for %s on node %d.", strerror(errno), node);
         return ret;
     }
    return ret;
}
