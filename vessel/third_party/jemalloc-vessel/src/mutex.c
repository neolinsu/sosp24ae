#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/assert.h"
#include "jemalloc/internal/malloc_io.h"
#include "jemalloc/internal/spin.h"

#ifndef _CRT_SPINCOUNT
#define _CRT_SPINCOUNT 4000
#endif

/*
 * Based on benchmark results, a fixed spin with this amount of retries works
 * well for our critical sections.
 */
int64_t opt_mutex_max_spin = 600;

/******************************************************************************/
/* Data. */

#ifdef JEMALLOC_LAZY_LOCK
bool isthreaded = false;
#endif
#ifdef JEMALLOC_MUTEX_INIT_CB
static bool		postpone_init = true;
static malloc_mutex_t	*postponed_mutexes = NULL;
#endif

/******************************************************************************/
/*
 * We intercept pthread_create() calls in order to toggle isthreaded if the
 * process goes multi-threaded.
 */

#if defined(JEMALLOC_LAZY_LOCK) && !defined(_WIN32)
JEMALLOC_EXPORT int
pthread_create(pthread_t *__restrict thread,
    const pthread_attr_t *__restrict attr, void *(*start_routine)(void *),
    void *__restrict arg) {
	return pthread_create_wrapper(thread, attr, start_routine, arg);
}
#endif

/******************************************************************************/

#ifdef JEMALLOC_MUTEX_INIT_CB
JEMALLOC_EXPORT int	_pthread_mutex_init_calloc_cb(pthread_mutex_t *mutex,
    void *(calloc_cb)(size_t, size_t));
#endif

#ifndef VESSEL_STARTUP
struct vessel_alloc_chunk_api_args {
    size_t chunk_num_i;
    void*  allocated_o;
};

typedef void(*apis_func_t)(void*);
struct vessel_cluster_spawn_api_args {
    apis_func_t func_i;
    void*  args_i;
    int pref_core_id_i;
    void* ret_o;
};

typedef void(*apis_func_t)(void*);
struct vessel_spawn_all_clusters_api_args {
    apis_func_t func_i;
    void*  args_i;
    int ret_o;
};

struct vessel_register_uipi_handlers_api_args {
    struct uipi_ops_inf *ops_i;
    int ret_o;
};

typedef void(*api_op_func_t)(void*);
struct vessel_apis {
    api_op_func_t alloc_chunk;
    api_op_func_t cluster_spawn;
    api_op_func_t cluster_spawn_all;
    api_op_func_t cluster_yield;
    api_op_func_t cluster_exit;
};

typedef struct vessel_apis vessel_apis_t;
const vessel_apis_t *vapis = (vessel_apis_t *) 0x8d232008;
#define __wrpkru_trusted(KEY) \
  do { \
  __label__ vessel_trusted; \
  vessel_trusted: \
    asm goto ( \
      "xor %%eax, %%eax\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%edx, %%edx\n\t" \
      ".byte 0x0f,0x01,0xef\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%ebx, %%ebx\n\t" \
      "RDPKRU\n\t" \
      "cmp %%ebx, %%eax\n\t" \
      "jne %l1\n\t" \
      ::"r"(KEY):"rax", "rcx", "rdx", "rbx", "memory": vessel_trusted \
    ); \
  } while(0)

#define __wrpkru_percpu(MEM_PTR) \
  do { \
  __label__ vessel_start; \
  vessel_start: \
    asm goto ( \
      "xor %%rcx, %%rcx\n\t" \
      "rdtscp\n\t" \
      "andq $0xFFF, %%rcx\n\t" \
      "movabsq $0x8d06a000, %%rax\n\t" \
      "mov (%%rax, %%rcx, 4), %%eax\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "xor %%edx, %%edx\n\t" \
      ".byte 0x0f,0x01,0xef\n\t" \
      "xor %%rcx, %%rcx\n\t" \
      "rdtscp\n\t" \
      "andq $0xFFF, %%rcx\n\t" \
      "movabsq $0x8d06a000, %%rax\n\t" \
      "mov (%%rax, %%rcx, 4), %%ebx\n\t" \
      "xor %%ecx, %%ecx\n\t" \
      "RDPKRU\n\t" \
      "cmp %%ebx, %%eax\n\t" \
      "jne %l1\n\t" \
      ::"r"(MEM_PTR):"rax", "rcx", "rdx", "rbx", "memory": vessel_start \
    ); \
  } while(0)

// Switching between isolated and application
#define erim_switch_to_trusted						\
  do {                                    \
    __wrpkru_trusted(0);	\
  } while(0)

#define erim_switch_to_untrusted(PTR) \
  do {                                \
    __wrpkru_percpu(PTR);             \
  } while(0)

int vessel_mutex_lock(spinlock_t * lock) {
	while(!spin_try_lock(lock)) {
		// erim_switch_to_trusted;
		// vapis->cluster_yield(NULL);
		// erim_switch_to_untrusted(0);
		while (lock->locked)
			cpu_relax();
	}
	return 0;
}
int vessel_mutex_unlock(spinlock_t * lock) {
	spin_unlock(lock);
	return 0;
}
int vessel_mutex_trylock(spinlock_t * lock) {
	return spin_try_lock(lock);
}
#endif

void
malloc_mutex_lock_slow(malloc_mutex_t *mutex) {
	mutex_prof_data_t *data = &mutex->prof_data;
	nstime_t before;

	if (ncpus == 1) {
		goto label_spin_done;
	}

	int cnt = 0;
	do {
		spin_cpu_spinwait();
		if (!atomic_load_b(&mutex->locked, ATOMIC_RELAXED)
                    && !malloc_mutex_trylock_final(mutex)) {
			data->n_spin_acquired++;
			return;
		}
	} while (cnt++ < opt_mutex_max_spin || opt_mutex_max_spin == -1);

	if (!config_stats) {
		/* Only spin is useful when stats is off. */
		malloc_mutex_lock_final(mutex);
		return;
	}
label_spin_done:
	nstime_init_update(&before);
	/* Copy before to after to avoid clock skews. */
	nstime_t after;
	nstime_copy(&after, &before);
	uint32_t n_thds = atomic_fetch_add_u32(&data->n_waiting_thds, 1,
	    ATOMIC_RELAXED) + 1;
	/* One last try as above two calls may take quite some cycles. */
	if (!malloc_mutex_trylock_final(mutex)) {
		atomic_fetch_sub_u32(&data->n_waiting_thds, 1, ATOMIC_RELAXED);
		data->n_spin_acquired++;
		return;
	}

	/* True slow path. */
	malloc_mutex_lock_final(mutex);
	/* Update more slow-path only counters. */
	atomic_fetch_sub_u32(&data->n_waiting_thds, 1, ATOMIC_RELAXED);
	nstime_update(&after);

	nstime_t delta;
	nstime_copy(&delta, &after);
	nstime_subtract(&delta, &before);

	data->n_wait_times++;
	nstime_add(&data->tot_wait_time, &delta);
	if (nstime_compare(&data->max_wait_time, &delta) < 0) {
		nstime_copy(&data->max_wait_time, &delta);
	}
	if (n_thds > data->max_n_thds) {
		data->max_n_thds = n_thds;
	}
}

static void
mutex_prof_data_init(mutex_prof_data_t *data) {
	memset(data, 0, sizeof(mutex_prof_data_t));
	nstime_init_zero(&data->max_wait_time);
	nstime_init_zero(&data->tot_wait_time);
	data->prev_owner = NULL;
}

void
malloc_mutex_prof_data_reset(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	malloc_mutex_assert_owner(tsdn, mutex);
	mutex_prof_data_init(&mutex->prof_data);
}

static int
mutex_addr_comp(const witness_t *witness1, void *mutex1,
    const witness_t *witness2, void *mutex2) {
	assert(mutex1 != NULL);
	assert(mutex2 != NULL);
	uintptr_t mu1int = (uintptr_t)mutex1;
	uintptr_t mu2int = (uintptr_t)mutex2;
	if (mu1int < mu2int) {
		return -1;
	} else if (mu1int == mu2int) {
		return 0;
	} else {
		return 1;
	}
}

bool
malloc_mutex_init(malloc_mutex_t *mutex, const char *name,
    witness_rank_t rank, malloc_mutex_lock_order_t lock_order) {
	mutex_prof_data_init(&mutex->prof_data);
#ifdef _WIN32
#  if _WIN32_WINNT >= 0x0600
	InitializeSRWLock(&mutex->lock);
#  else
	if (!InitializeCriticalSectionAndSpinCount(&mutex->lock,
	    _CRT_SPINCOUNT)) {
		return true;
	}
#  endif
#elif (defined(JEMALLOC_OS_UNFAIR_LOCK))
       mutex->lock = OS_UNFAIR_LOCK_INIT;
#elif (defined(JEMALLOC_MUTEX_INIT_CB))
	if (postpone_init) {
		mutex->postponed_next = postponed_mutexes;
		postponed_mutexes = mutex;
	} else {
		if (_pthread_mutex_init_calloc_cb(&mutex->lock,
		    bootstrap_calloc) != 0) {
			return true;
		}
	}
#elif (!defined(VESSEL_STARTUP))
	spin_lock_init(&mutex->lock);
#else
	pthread_mutexattr_t attr;

	if (pthread_mutexattr_init(&attr) != 0) {
		return true;
	}
	pthread_mutexattr_settype(&attr, MALLOC_MUTEX_TYPE);
	if (pthread_mutex_init(&mutex->lock, &attr) != 0) {
		pthread_mutexattr_destroy(&attr);
		return true;
	}
	pthread_mutexattr_destroy(&attr);
#endif
	if (config_debug) {
		mutex->lock_order = lock_order;
		if (lock_order == malloc_mutex_address_ordered) {
			witness_init(&mutex->witness, name, rank,
			    mutex_addr_comp, mutex);
		} else {
			witness_init(&mutex->witness, name, rank, NULL, NULL);
		}
	}
	return false;
}

void
malloc_mutex_prefork(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	malloc_mutex_lock(tsdn, mutex);
}

void
malloc_mutex_postfork_parent(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	malloc_mutex_unlock(tsdn, mutex);
}

void
malloc_mutex_postfork_child(tsdn_t *tsdn, malloc_mutex_t *mutex) {
#ifdef JEMALLOC_MUTEX_INIT_CB
	malloc_mutex_unlock(tsdn, mutex);
#else
	if (malloc_mutex_init(mutex, mutex->witness.name,
	    mutex->witness.rank, mutex->lock_order)) {
		malloc_printf("<jemalloc>: Error re-initializing mutex in "
		    "child\n");
		if (opt_abort) {
			abort();
		}
	}
#endif
}

bool
malloc_mutex_boot(void) {
#ifdef JEMALLOC_MUTEX_INIT_CB
	postpone_init = false;
	while (postponed_mutexes != NULL) {
		if (_pthread_mutex_init_calloc_cb(&postponed_mutexes->lock,
		    bootstrap_calloc) != 0) {
			return true;
		}
		postponed_mutexes = postponed_mutexes->postponed_next;
	}
#endif
	return false;
}
