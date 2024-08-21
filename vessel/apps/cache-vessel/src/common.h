#include <stdint.h>
long long L3_size = 32 * 1024 * 1024;
void *L3_cache;
#ifndef PID
// report an error
#error "PID is not defined"
#else
const int pid = PID;
#endif
static unsigned int g_seed = 114514;

// Used to seed the generator.

// Compute a pseudorandom integer.
// Output value in range [0, 32767]
static inline int rand64(void)
{
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}
inline void clflush(void *p)
{
    asm volatile("clflush (%0)" ::"r"(p));
}
inline void mfence()
{
    asm volatile("mfence");
}

const long long total = 1000000000ll;
int randoms[1048576 * 16];
int read_cnt;
inline int myrand(void)
{
    return randoms[(read_cnt++) & (1048576 * 16 - 1)];
}
static void gen_rand()
{
    for (int i = 0; i < 1048576 * 16; i++)
    {
        randoms[i] = rand64();
    }
}
inline char *get_addr()
{
#ifdef AWARE
    if (pid == 1)
    {
        int set = myrand() & 7;
        int idx = (myrand() & (per_app_sets - 1));
        
        return (char *)(L3_cache) + set * 4096 + idx * allocate_size + 384 + per_app_sets * allocate_size;
    }
    if (pid == 2)
    {
        int set = myrand() & 7;
        int idx = (myrand() & (per_app_sets - 1));
        return (char *)(L3_cache) + (set * 4096) + idx * allocate_size + 384;
    }
#else
    int set = myrand() & 15;
    int idx = myrand() & (per_app_sets - 1);
    return (char *)L3_cache + set * 4096 + idx * allocate_size + 384;
#endif
}
inline void *get_source()
{
}