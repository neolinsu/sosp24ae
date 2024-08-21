#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mt19937_64.h"

static inline double gen_exponential(double mean) {
    return  - (1.0 / mean) * log(genrand64_real3());
}

enum DistributionType {
    Exponential,
};

typedef void* (*gen_func_t)(void*);

static inline void gen_rand_seq(
    size_t seq_length,
    size_t ele_size,
    gen_func_t gen_func,
    void* buf)
{
    uint64_t start = 0;
    memcpy(buf, gen_func(&start), ele_size);
    for (int i=1; i< seq_length; ++i) {
        void*dst = gen_func(buf);
        buf+=ele_size;
        memcpy(buf, dst, ele_size);
    }

}
