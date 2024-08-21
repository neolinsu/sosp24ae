
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <linux/perf_event.h>    /* Definition of PERF_* constants */
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>

#include <base/cpu.h>
#include <core/pmc.h>

struct rdpmc_ctx {
	int fd;
	struct perf_event_mmap_page *buf;
};

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
    int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
    return ret;
}

/**
 * rdpmc_open_attr - initialize a raw ring 3 readable performance counter
 * @attr: perf struct %perf_event_attr for the counter
 * @ctx:  Pointer to struct %rdpmc_ctx that is initialized.
 * @leader_ctx: context of group leader or NULL
 *
 * This allows more flexible setup with a custom &perf_event_attr.
 * For simple uses rdpmc_open() should be used instead.
 * Must be called for each thread using the counter.
 * Must be closed with rdpmc_close()
 */
int rdpmc_open_attr(struct perf_event_attr *attr, struct rdpmc_ctx *ctx,
		    struct rdpmc_ctx *leader_ctx, int cpu)
{
	ctx->fd = perf_event_open(attr, 0, cpu,
			  leader_ctx ? leader_ctx->fd : -1, 0);
	if (ctx->fd < 0) {
		perror("perf_event_open");
		return -1;
	}
	ctx->buf = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, ctx->fd, 0);
	if (ctx->buf == MAP_FAILED) {
		close(ctx->fd);
		perror("mmap on perf fd");
		return -1;
	}
	return 0;
}

int rdpmc_open(struct rdpmc_ctx *ctx, int cpu)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.type = PERF_TYPE_HARDWARE;
    attr.size = 128;

    attr.config = PERF_COUNT_HW_CACHE_MISSES;
    attr.config1 = 0;

    attr.sample_type = PERF_SAMPLE_IDENTIFIER;
    attr.disabled = 0;
    attr.exclude_user = 0;
    attr.exclude_kernel = 1;

	// struct perf_event_attr attr = {
	// 	.type = counter > 10 ? PERF_TYPE_RAW : PERF_TYPE_HARDWARE,
	// 	.size = PERF_ATTR_SIZE_VER0,
	// 	.config = counter,
	// 	.sample_type = PERF_SAMPLE_READ,
	// 	.exclude_kernel = 1,
	// };
	return rdpmc_open_attr(&attr, ctx, NULL, cpu);
}

int pmc_init_perthread(void) {
    int ret;
    struct rdpmc_ctx ctx;
	// ret = rdpmc_open(&ctx, 0x12A, 0x94000800, get_cur_cpuid());
	ret = rdpmc_open(&ctx, get_cur_cpuid());
    assert(ret==0);
    return ret;
}