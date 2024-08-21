#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */
#include <sys/ipc.h>
#include <sys/shm.h>

#include <unistd.h>
#include "common.h"
#include <emmintrin.h>

static inline uint32_t dk_random_next(uint64_t *state)
{
    (*state) = (*state) * 6364136223846793005ull + 1442695040888963407ull;
    return ((*state) >> 33);
}

static inline uint64_t get_idx(uint32_t r, uint64_t mask)
{
    return r & (~63) & (mask);
}

static inline void nt_store(char *buf, int c)
{
    __m128i a = _mm_set_epi32(c, c, c, c);
    _mm_store_si128((__m128i *)buf, a);
    _mm_store_si128((__m128i *)(buf + 16), a);
    _mm_store_si128((__m128i *)(buf + 32), a);
    _mm_store_si128((__m128i *)(buf + 48), a);
    // _mm_stream_si128((__m128i *)buf     , a);
    // _mm_stream_si128((__m128i *)(buf+16), a);
    // _mm_stream_si128((__m128i *)(buf+32), a);
    // _mm_stream_si128((__m128i *)(buf+48), a);
}

struct performance_log_entry *g_performance_log;

void *controller;
int main(int argc, char **argv)
{
    gen_rand();
    mfence();
    uint64_t rand_state = rdtsc();
    size_t area_size = 512ll * 1024 * 1024; // 512 MB
    // align it up to 1MB
    int c = (int)rand_state;
    char *buf = aligned_alloc(64, area_size);
    memset(buf, 0, area_size);
    for (int i = 0; i < 1000; i++)
    {
        for (uint64_t batch_i = 0; batch_i < (4096 >> 2); ++batch_i)
        {
            int nxt = dk_random_next(&rand_state) & (area_size / 64 - 1);
            char *ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
        }
    }
    unsigned long start_time = rdtsc();
    // find veiokerneld by get the return code of ps aux | grep veiokerned -q
    if (access("bandwidth.config", F_OK) == 0)
    {
        int bandwidth = 0;
        freopen("bandwidth.config", "r", stdin);
        scanf("%d", &bandwidth);
        bandwidth -= dk_random_next(&rand_state) % 5 + 2;
        float time_s = 1.0 / 0.15355451561798175;
        time_s = time_s * 100 / bandwidth;
        usleep(time_s * 1000 * 1000);
        unsigned long end = rdtsc();
        printf("             %lu    start_time\n", start_time);
        printf("             %lu    end_time\n", end);
        exit(0);
    }
    for (int i = 0; i < 100000; i++)
    {
        for (uint64_t batch_i = 0; batch_i < (4096 >> 2); ++batch_i)
        {
            int nxt = dk_random_next(&rand_state) & (area_size / 64 - 1);
            char *ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
            ptr = buf + (nxt << 6);
            nt_store(ptr, c);
            nxt++;
        }
    }
    unsigned long end = rdtsc();
    printf("             %lu    start_time\n", start_time);
    printf("             %lu    end_time\n", end);
}