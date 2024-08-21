#pragma once

#include <linux/perf_event.h>

struct rdpmc_ctx {
	int fd;
	struct perf_event_mmap_page *buf;
};