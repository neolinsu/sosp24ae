#pragma once
#include <stdint.h>
#include <net_utils.h>
#include <rand.h>
#include <client.h>
#include <config.h>

static inline uint64_t gen_zero(void*args) {
    return 0;
}

static uint64_t _key_range;
static inline void set_key_range(uint64_t range) {
    _key_range = range;
}
static inline uint64_t key_gen_uniform(void*args) {
    uint64_t r = genrand64_int64();
    r %= _key_range;
    return  r;
}
static uint64_t _key_start;
static inline void set_key_start(uint64_t start) {
    _key_start = start;
}
static inline uint64_t key_gen_acc(void*args) {
    uint64_t r = _key_start++;
    return  r;
}

static inline uint8_t op_gen_w10(void*args) {
    return Set;
}

static inline uint8_t op_gen_w1r9(void*args) {
    if (genrand64_real3() <0.1) {
        return Set;
    } else {
        return Get;
    }
}

static double _time_gen_exponential_mean;
static inline void set_time_gen_exponential_mean(double mean) {
    _time_gen_exponential_mean = mean;
}
static inline uint64_t time_gen_exponential_accu(void* buf) {
    uint64_t last = *((uint64_t*)buf);
    last += (uint64_t)round(gen_exponential(_time_gen_exponential_mean)*clock);
    return last;
}
