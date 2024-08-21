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
#include "common.h"


void *controller;
int main(int argc, char **argv)
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
    // we use huge page to allocate a 1GB page
    L3_cache = aligned_alloc(1024*1024*1024, 1024*1024*1024); // mmap(NULL, L3_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
    // mmap(NULL, L3_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
    if (L3_cache == NULL)
    {
        abort();
    }
    memset(L3_cache, 0, L3_size);
    unsigned long start_spin = *(volatile unsigned long *)controller;
    while (1)
    {
        if (rdtsc() - start_spin > 2100*1000*1000ll)
        {
            break;
        }
    }
    mfence();
    // align it up to 1MB
    unsigned long start_time = rdtsc();
    for (int i = 0; i < total; i++)
    {
        char *start = get_addr();
        char *dst = get_addr();
        memcpy(dst, start, object_size);
        if((i&65535)==0){
            sched_yield();
        }
    }
    unsigned long end = rdtsc();
    printf("             %lu    start_time\n", start_time);
    printf("             %lu    end_time\n",end);
}