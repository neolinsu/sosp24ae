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
    for (int i = 0; i < 1000; i++)
    {
        vessel_cluster_yield();
    }
    controller = shmat(fd, NULL, 0);
    if (controller == (void *)-1)
    {
        perror("mmap");
    }
    memset(controller, 0, 4096);
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
    int cnt=1;
    while(true)
    {
        vessel_cluster_yield();
        cnt++;
    }
}

int main(int argc, char *argv[])
{
    real_main(NULL);
    return 0;
}
