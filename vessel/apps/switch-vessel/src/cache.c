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

struct real_main_args {
    int argc;
    void* argv;
};

void real_main(void* a)
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
    for (int i = 0; i < 1000; i++)
    {
        vessel_cluster_yield();
    }

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
    unsigned long *res_list = (unsigned long *) malloc(100000*sizeof(res_list));
    for (int i = 0; i < 100000; i++)
    {
        unsigned long start = rdtsc();
        vessel_cluster_yield();
        unsigned long end = rdtsc();
        res_list[i] = end - start;
    }
    FILE *fp = NULL;
    fp = fopen("./res.csv", "w+");
    if(fp==NULL) abort();
    for (int i = 0; i < 100000; i++)
    {
        fprintf(fp, "%ld,\n", res_list[i]);
    }
    fclose(fp);
    return;
}

int main(int argc, char *argv[])
{
    real_main(NULL);
    return 0;
}
