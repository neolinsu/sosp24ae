/*
 * stack.c - allocates and manages per-thread stacks
 */

#include <sys/mman.h>

#include <base/stddef.h>
#include <base/lock.h>
#include <base/atomic.h>
#include <base/limits.h>
#include <base/log.h>

#include <runtime/minialloc.h>
#include <runtime/mmem/page.h>
#include <runtime/tls/thread.h>
#include <runtime/tls/tcache.h>

#include "defs.h"

static struct tcache *stack_tcache;
DEFINE_PERTHREAD(struct tcache_perthread, stack_pt);

static struct stack *stack_create()
{
	void *stack_addr;
	struct stack *s;

	stack_addr = mini_malloc_4K(align_up(sizeof(struct stack), PGSIZE_4KB));
	memset(stack_addr, 0,sizeof(struct stack));
	if (!stack_addr)
		return NULL;

	s = (struct stack *)stack_addr;
	if (mprotect(s->guard, RUNTIME_GUARD_SIZE, PROT_NONE) == - 1) {
		log_err("stack_create -> mprotect");
		abort();
		return NULL;
	}
	return s;
}

/* WARNING: the contents of the stack may be lost after reclaiming. */
static void stack_reclaim(struct stack *s)
{
	// TODO
	WARN_ON_ONCE(false);
}

static DEFINE_SPINLOCK(stack_lock);
static int free_stack_count;
static struct stack *free_stacks[RUNTIME_MAX_THREADS];

static void stack_tcache_free(struct tcache *tc, int nr, void **items)
{
	int i;

	/* try to release the backing memory first */
	for (i = 0; i < nr; i++)
		stack_reclaim((struct stack *)items[i]);

	/* then make the stacks available for reallocation */
	spin_lock(&stack_lock);
	for (i = 0; i < nr; i++)
		free_stacks[free_stack_count++] = items[i];
	BUG_ON(free_stack_count >=
	       RUNTIME_MAX_THREADS + TCACHE_DEFAULT_MAG_SIZE);
	spin_unlock(&stack_lock);
}

static int stack_tcache_alloc(struct tcache *tc, int nr, void **items)
{
	int i = 0;

	spin_lock(&stack_lock);
	while (free_stack_count && i < nr) {
		items[i++] = free_stacks[--free_stack_count];
	}
	spin_unlock(&stack_lock);


	for (; i < nr; i++) {
		items[i] = stack_create();
		if (unlikely(!items[i]))
			goto fail;
	}

	return 0;

fail:
	log_err_ratelimited("stack: failed to allocate stack memory");
	stack_tcache_free(tc, i, items);
	return -ENOMEM;
}

static const struct tcache_ops stack_tcache_ops = {
	.alloc	= stack_tcache_alloc,
	.free	= stack_tcache_free,
};

/**
 * stack_init_thread - intializes per-thread state
 * Returns 0 (always successful).
 */
int stack_init_thread(void)
{
	tcache_init_perthread(stack_tcache, &perthread_get(stack_pt));
	return 0;
}

/**
 * stack_init - initializes the stack allocator
 * Returns 0 if successful, or -ENOMEM if out of memory.
 */
int stack_init(void)
{
	stack_tcache = tcache_create("runtime_stacks", &stack_tcache_ops,
				     TCACHE_DEFAULT_MAG_SIZE,
				     RUNTIME_STACK_SIZE);
	if (!stack_tcache)
		return -ENOMEM;
	return 0;
}
