#include <stdlib.h>
#include <base/log.h>

#include "uexec/uexec.h"

#include "arch_jump.h"

inline void __attribute ((noreturn)) jump_with_stack(size_t dest, size_t *stack)
{
	JUMP_WITH_STACK(dest, stack);
	// If we didn't jump, something went wrong
	log_err("jump_with_stack -> wrong branch!\n");
	abort();
}