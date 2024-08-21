/*
 * thread.c - support for thread-local storage and initialization
 */
#include <x86gprintrin.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>

#include <base/stddef.h>
#include <base/log.h>
#include <base/cpu.h>
#include <base/lock.h>

#include <runtime/mmem/mmem.h>
#include <runtime/minialloc.h>
#include <runtime/tls/thread.h>

/* protects thread_count */
static DEFINE_SPINLOCK(thread_lock);

unsigned int thread_count;
void *perthread_offsets[NVTHREAD];
__thread void *perthread_ptr;

__thread unsigned int thread_numa_node;
__thread unsigned int thread_id;
__thread bool thread_init_done;

extern const char __perthread_start[];
extern const char __perthread_end[];

static int thread_alloc_perthread(void)
{
	void *addr;
	size_t len = __perthread_end - __perthread_start;

	/* no perthread data */
	if (!len)
		return 0;

    // TODO
	// addr = mem_map_anom(NULL, len, PGSIZE_4KB, thread_numa_node);
	// if (addr == MAP_FAILED)
	// 	return -ENOMEM;
    addr = mini_malloc_4K(align_up(len, PGSIZE_4KB));
	log_debug("thread_alloc_perthread->addr:%p", addr);
	memset(addr, 0, align_up(len, PGSIZE_4KB));
	perthread_ptr = addr;
	perthread_offsets[my_thread_id()] = addr;
	return 0;
}

/**
 * thread_get_sys_tid - gets the tid of the current kernel thread,
 * not vessel thread.
 */
pid_t thread_get_sys_tid(void)
{
#ifndef SYS_gettid
	#error "SYS_gettid unavailable on this system"
#endif
	return syscall(SYS_gettid);
}

extern int numa_nodeid;

/**
 * thread_init_thread - initializes a thread
 *
 * Returns 0 if successful, otherwise fail.
 */
int thread_init_thread(void)
{
	int ret;

	spin_lock(&thread_lock);
	if (thread_count >= NVTHREAD) {
		spin_unlock(&thread_lock);
		log_err("thread: hit thread limit of %d\n", NVTHREAD);
		return -ENOSPC;
	}
	thread_id = thread_count++;
	spin_unlock(&thread_lock);

	thread_numa_node = numa_nodeid;

	ret = thread_alloc_perthread();
	if (ret)
		return ret;

	return 0;
}
