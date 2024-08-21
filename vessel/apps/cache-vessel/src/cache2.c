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


#include <runtime/runtime.h>
#include <runtime/thread.h>
#include <runtime/simple_sync.h>
#include <runtime/thread.h>

#include <interface.h>
#include "common.h"

void *controller;
void real_main(void *a)
{
    gen_rand();
    int fd = shmget(114514, 4096, IPC_CREAT | 0777);
    if (fd < 0)
    {
        perror("shm_open");
    }
    // attach to shared memory
    controller = shmat(fd, NULL, 0);
    if (controller == (void *)-1)
    {
        perror("mmap");
    }
    memset(controller, 0, 4096);
    // we use huge page to allocate a 1GB page
    // L3_cache = mmap(NULL, L3_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
    L3_cache = aligned_alloc(1024*1024*1024, 1024*1024*1024); // mmap(NULL, L3_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);

    if (L3_cache == NULL)
    {
        abort();
    }
    memset(L3_cache, 0, L3_size);
    *(volatile unsigned long *)controller = rdtsc();
    unsigned long start_spin = *(volatile unsigned long *)controller;
    while (1)
    {
        if (rdtsc() - start_spin > 2100*1000*1000ll)
        {
            break;
        }
        vessel_cluster_yield();
    }
    mfence();
    // align it up to 1MB
    unsigned long start_time = rdtsc();
    for (int i = 0; i < total; i++)
    {
        char *start = get_addr();
        char *dst = get_addr();
        memcpy(dst, start, object_size);
        if(((65535) & (i))==0){
            vessel_cluster_yield();
        }
    }
    unsigned long end = rdtsc();
    vessel_write_log("{\"start_time\":%ld, \"end_time\":%ld}", start_time, end);
    *(volatile unsigned long *)controller = -1;
    while (*(volatile unsigned long *)controller!= -2) {
        vessel_cluster_yield();
    }
}

int main(int argc, char *argv[])
{
    real_main(NULL);
    return 0;
}
