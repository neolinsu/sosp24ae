/*
 * page.c - the page allocator
 */

#include <sys/mman.h>

#include <vessel/interface.h>


#include <base/lock.h>
#include <base/list.h>
#include <base/cpu.h>
#include <base/log.h>

#include <runtime/minialloc.h>
#include <runtime/mmem/page.h>
#include <runtime/mmem/slab.h>
#include <runtime/tls/tcache.h>
#include <runtime/tls/thread.h>

/*
 * This pointer contains an array of page structs, organized as follows:
 * [NUMA 0 pages] [NUMA 1 pages] ... [NUMA N pages]
 */
struct page *page_tbl;
unsigned long ve_page_base_addr;

/* large page (2MB) definitions */
struct lgpage_node {
	spinlock_t		lock;
	unsigned int		idx;
	struct page		*tbl; /* aliases page_tbl above */
	struct list_head	pages;
	uint64_t		pad[4];
} __aligned(CACHE_LINE_SIZE);
static struct lgpage_node lgpage_nodes[NNUMA];

/* small page (4KB) definitions */
extern struct slab smpage_slab; /* defined in mm/slab.c */
extern struct tcache *smpage_tcache;
static __thread struct tcache_perthread smpage_pt;

#ifdef DEBUG

static void page_check(struct page *pg, size_t pgsize)
{
	/* since the page is allocated, it must be marked in use */
	assert(pg->flags & PAGE_FLAG_IN_USE);

	/* check for unsupported page sizes */
	assert(pgsize == PGSIZE_4KB || pgsize == PGSIZE_2MB);

	/* finally verify the page is configured correctly for its size */
	assert(page_to_size(pg) == pgsize);
	if (pgsize == PGSIZE_4KB) {
		assert(!(pg->flags & PAGE_FLAG_SHATTERED));
		pg = smpage_to_lgpage(pg);
		assert(pg->flags & PAGE_FLAG_LARGE);
		assert(pg->flags & PAGE_FLAG_SHATTERED);
	}

	/* check that the lgpage is inside the table */
	assert(pg - page_tbl >= 0 &&
	       pg - page_tbl < LGPAGE_META_ENTS * NNUMA);
}

static void page_alloc_check(struct page *pg, size_t pgsize)
{
	page_check(pg, pgsize);
	assert(!kref_released(&pg->ref));

	/* poison the page */
	memset(page_to_addr(pg), 0xEF, pgsize);
}

static void page_free_check(struct page *pg, size_t pgsize)
{
	page_check(pg, pgsize);
	assert(kref_released(&pg->ref));

	/* poison the page */
	memset(page_to_addr(pg), 0x89, pgsize);
}

#else /* DEBUG */

static void page_alloc_check(struct page *pg, size_t pgsize) {;}
static void page_free_check(struct page *pg, size_t pgsize) {;}

#endif /* DEBUG */

static int lgpage_create(struct page *pg, int numa_node)
{
	void *pgaddr = lgpage_to_addr(pg);

	touch_mapping(pgaddr, PGSIZE_2MB, PGSIZE_2MB);
	//bind_mem_on_node(pgaddr, PGSIZE_2MB, numa_node);

	kref_init(&pg->ref);
	pg->flags = PAGE_FLAG_LARGE | PAGE_FLAG_IN_USE;
	return 0;
}

static void lgpage_destroy(struct page *pg)
{
	pg->flags = 0;
	pg->paddr = 0;
}

static struct page *lgpage_alloc_on_node(int numa_node)
{
	struct lgpage_node *node;
	struct page *pg;
	int ret;

	assert(numa_node < NNUMA);
	node = &lgpage_nodes[numa_node];

	spin_lock(&node->lock);
	pg = list_pop(&node->pages, struct page, link);
	if (!pg) {
		if (unlikely(node->idx >= LGPAGE_META_ENTS)) {
			spin_unlock(&node->lock);
			log_err_once("out of page region addresses");
			return NULL;
		}

		pg = &node->tbl[node->idx++];
	}
	spin_unlock(&node->lock);

	assert(!(pg->flags & PAGE_FLAG_IN_USE));
	ret = lgpage_create(pg, numa_node);
	if (ret) {
		log_err_once("page: unable to create 2MB page,"
			     "node = %d, ret = %d", numa_node, ret);
		return NULL;
	}

	return pg;
}

static void lgpage_free(struct page *pg)
{
	unsigned int numa_node = addr_to_numa_node(lgpage_to_addr(pg));
	struct lgpage_node *node = &lgpage_nodes[numa_node];

	lgpage_destroy(pg);
	spin_lock(&node->lock);
	list_add(&node->pages, &pg->link);
	spin_unlock(&node->lock);
}

static struct page *smpage_alloc_on_node(int numa_node)
{
	struct page *pg;
	void *addr;

	if (is_thread_init_done() && my_thread_numa_node() == numa_node) {
		/* if on the local node use the fast path */
		struct tcache_perthread *smpage_pt_ptr = &smpage_pt;
		addr = tcache_alloc(smpage_pt_ptr);
	} else {
		/* otherwise perform a remote slab allocation */
		addr = slab_alloc_on_node(&smpage_slab, numa_node);
	}

	if (!addr)
		return NULL;

	pg = addr_to_smpage(addr);
	kref_init(&pg->ref);
	pg->flags = PAGE_FLAG_IN_USE;
	pg->paddr = addr_to_pa(addr);
	return pg;
}

static void smpage_free(struct page *pg)
{
	void *addr = smpage_to_addr(pg);
	unsigned int numa_node = addr_to_numa_node(addr);

	pg->flags = 0;

	if (is_thread_init_done() && my_thread_numa_node() == numa_node) {
		/* if on the local node use the fast path */
		struct tcache_perthread *smpage_pt_ptr = &smpage_pt;
		tcache_free(smpage_pt_ptr, addr);
	} else {
		/* otherwise perform a remote slab free */
		slab_free(&smpage_slab, addr);
	}
}

/**
 * page_alloc_on_node - allocates a page for a NUMA node
 * @pgsize: the size of the page
 * @numa_node: the NUMA node the page is allocated from
 *
 * Returns a page, or NULL if an error occurred.
 */
struct page *page_alloc_on_node(size_t pgsize, int numa_node)
{
	struct page *pg;

	switch(pgsize) {
	case PGSIZE_4KB:
		pg = smpage_alloc_on_node(numa_node);
		break;
	case PGSIZE_2MB:
		pg = lgpage_alloc_on_node(numa_node);
		break;
	default:
		/* unsupported page size */
		pg = NULL;
	}

	page_alloc_check(pg, pgsize);
	return pg;
}

/**
 * page_alloc - allocates a page
 * @pgsize: the size of the page
 *
 * Returns a page, or NULL if out of memory.
 */
struct page *page_alloc(size_t pgsize)
{
	return page_alloc_on_node(pgsize, my_thread_numa_node());
}

/**
 * page_zalloc - allocates a zeroed page
 * @pgsize: the size of the page
 *
 * Returns a page, or NULL if out of memory.
 */
struct page *page_zalloc(size_t pgsize)
{
	void *addr;
	struct page *pg = page_alloc(pgsize);
	if (!pg)
		return NULL;

	addr = page_to_addr(pg);
	memset(addr, 0, pgsize);
	return pg;
}

/**
 * page_alloc_addr_on_node - allocates a page address on for a NUMA node
 * @pgsize: the size of the page
 * @numa_node: the NUMA node the page is allocated from
 *
 * Returns a pointer to page data, or NULL if an error occurred.
 */
void *page_alloc_addr_on_node(size_t pgsize, int numa_node)
{
	struct page *pg = page_alloc_on_node(pgsize, numa_node);
	if (!pg)
		return NULL;
	return page_to_addr(pg);
}

/**
 * page_alloc_addr - allocates a page address
 * @pgsize: the size of the page
 *
 * Returns a pointer to page data, or NULL if an error occurred.
 */
void *page_alloc_addr(size_t pgsize)
{
	return page_alloc_addr_on_node(pgsize, my_thread_numa_node());
}

/**
 * page_zalloc_addr_on_node - allocates a zeroed page address for a NUMA node
 * @pgsize: the size of the page
 * @numa_node: the NUMA node the page is allocated from
 *
 * Returns a pointer to zeroed page data, or NULL if an error occurred.
 */
void *page_zalloc_addr_on_node(size_t pgsize, int numa_node)
{
	void *addr = page_alloc_addr_on_node(pgsize, numa_node);
	if (addr)
		memset(addr, 0, pgsize);
	return addr;
}

/**
 * page_alloc_addr - allocates a page address
 * @pgsize: the size of the page
 *
 * Returns a pointer to zeroed page data, or NULL if an error occurred.
 */
void *page_zalloc_addr(size_t pgsize)
{
	void *addr = page_alloc_addr(pgsize);
	if (addr)
		memset(addr, 0, pgsize);
	return addr;
}

/**
 * page_put_addr - decrements underlying page's reference count
 * @addr: a pointer to the page data
 */
void page_put_addr(void *addr)
{
	assert(is_page_addr(addr));
	page_put(addr_to_page(addr));
}

/**
 * page_release - frees a page
 * @kref: the embedded kref struct inside a page
 */
void page_release(struct kref *ref)
{
	struct page *pg = container_of(ref, struct page, ref);
	size_t pgsize = page_to_size(pg);
	page_free_check(pg, pgsize);

	switch(pgsize) {
	case PGSIZE_4KB:
		smpage_free(pg);
		break;
	case PGSIZE_2MB:
		lgpage_free(pg);
		break;
	default:
		/* unsupported page size */
		panic("page: tried to free an invalid page size %ld", pgsize);
	}
}

/**
 * page_init - initializes the page subsystem
 */
int page_init(void)
{
	struct lgpage_node *node;
	void *addr;
	int i;
	// Allocate page system for runtime.
	assert((LGPAGE_NODE_ADDR_LEN * 2) % (1 * GB) == 0);
	ve_page_base_addr = (unsigned long)vessel_alloc_huge_chunk((LGPAGE_NODE_ADDR_LEN * 2) / (1 * GB)); // aligned_alloc(PGSIZE_2MB, LGPAGE_NODE_ADDR_LEN * NNUMA);
	log_debug("Page init -> ve_page_base_addr: %p", (void*)ve_page_base_addr);
	log_debug("Page init -> ve_page_base_addr len: %llu", (LGPAGE_NODE_ADDR_LEN * 2) / (1 * GB));
	/* First reserve address-space for the page table. */
	addr = mini_malloc(align_up(LGPAGE_META_LEN * 2, PGSIZE_2MB));
	memset(addr, 0, align_up(LGPAGE_META_LEN * 2, PGSIZE_2MB));
	log_debug("Page init -> addr: %p", addr);
	if (!addr || !ve_page_base_addr) {
		return -ENOMEM;
	}

	/* Then map NUMA-local large pages on top. */
	bitmap_for_each_set(cpu_info_tbl.numa_mask, NNUMA, i) {
		node = &lgpage_nodes[i];
		// Allocate the table for each node.
		node->tbl = addr + i * LGPAGE_META_LEN;

		spin_lock_init(&node->lock);
		list_head_init(&node->pages);
		node->idx = 0;
	}

	page_tbl = addr;
	return 0;
}

/**
 * page_init_thread - initializes the page subsystem for a thread
 */
int page_init_thread(void)
{
	struct tcache_perthread *smpage_pt_ptr = &smpage_pt;
	tcache_init_perthread(smpage_tcache, smpage_pt_ptr);
	return 0;
}
