#include <stddef.h>
#include <stdint.h>

extern void *
mini_malloc (size_t n);

extern void *
mini_malloc_C (size_t n);

extern void *
mini_malloc_4K (size_t n);

extern void
mini_free (void *ptr);