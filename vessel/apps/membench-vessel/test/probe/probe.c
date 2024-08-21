#include <unistd.h>
#include <assert.h>
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
#include <emmintrin.h>

#include "probe.h"

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
		    struct rdpmc_ctx *leader_ctx)
{
	ctx->fd = perf_event_open(attr, 1338046, -1,
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

int rdpmc_open(struct rdpmc_ctx *ctx, unsigned long long config, unsigned long long config1)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.type = PERF_TYPE_RAW;
    attr.size = 128;

    attr.config = config;
    attr.config1 = config1;

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
	return rdpmc_open_attr(&attr, ctx, NULL);
}

#define rmb() asm volatile("" ::: "memory")

/**
 * rdpmc_read - read a ring 3 readable performance counter
 * @ctx: Pointer to initialized &rdpmc_ctx structure.
 *
 * Read the current value of a running performance counter.
 * This should only be called from the same thread/process as opened
 * the context. For new threads please create a new context.
 */
unsigned long long rdpmc_read(unsigned index)
{
	uint64_t val = _rdpmc(index);
	rmb();
	return (val + 0x7ffffffffffflu) & 0xfffffffffffflu;
}

static inline
void nt_store(char* buf, int c) {
    __m128i a = _mm_set_epi32(c, c, c, c);
    _mm_stream_si128((__m128i *)buf     , a);
    _mm_stream_si128((__m128i *)(buf+16), a);
    _mm_stream_si128((__m128i *)(buf+32), a);
    _mm_stream_si128((__m128i *)(buf+48), a);
}

static inline uint64_t rdtsc(void)
{
	uint32_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((uint64_t)a) | (((uint64_t)d) << 32);
}

void rdpmc_close(struct rdpmc_ctx *ctx)
{
	close(ctx->fd);
	munmap(ctx->buf, sysconf(_SC_PAGESIZE));
}

int main (int argc, char *argv[]) {
	int ret;
	struct rdpmc_ctx ctx;
	ret = rdpmc_open(&ctx, 0x12A, 0x94000800);
	assert(ret==0);
	#define BUF_SIZE 64 * 4096
	char* buf = aligned_alloc(64, BUF_SIZE);
	memset(buf, -1, BUF_SIZE);
	for (int c=0; c<BUF_SIZE/64; ++c) {
		_mm_clflush(buf + 64 * c);
	}
	_mm_mfence();
	int value = rdtsc();
	for (int r = 0; r < 3; r++)
		for (int c=0; c<64; ++c) {
			nt_store(buf + c * 64, value);
			rmb();	
		}
	_mm_mfence();
	printf("res: %llu\n", rdpmc_read(0));
	value = rdtsc();
	for (int r = 0; r < 3; r++)
		for (int c=0; c<64; ++c) {
			nt_store(buf + c * 64, value);
			rmb();	
		}
	_mm_mfence();
	printf("res: %llu\n", rdpmc_read(1));
	rdpmc_close(&ctx);
}