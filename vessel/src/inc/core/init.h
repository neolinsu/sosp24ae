/*
 * init.h - support for initialization
 */

#pragma once
#include <unistd.h>
#include <base/stddef.h>
#include <base/bitmap.h>

struct init_handler {
	const char *name;
	int (*init)(void);
};

#define __REGISTER_INIT_HANDLER(func, level)				\
	static struct init_handler __init_call##level##func __used	\
	__attribute__((section(".initcall" #level))) =			\
	{__str(func), func}

/* normal initialization */
#define REGISTER_EARLY_INIT(func)	__REGISTER_INIT_HANDLER(func, 0)
#define REGISTER_NORMAL_INIT(func)	__REGISTER_INIT_HANDLER(func, 1)
#define REGISTER_LATE_INIT(func)	__REGISTER_INIT_HANDLER(func, 2)

/* per-thread initialization */
#define REGISTER_THREAD_INIT(func)	__REGISTER_INIT_HANDLER(func, t)

extern int core_init(void);
extern int task_init(bitmap_ptr_t affinitive_cpus, int exclusive, char* path, int argc, char** argv, char** env, uid_t uid);
extern void init_shutdown(int status) __noreturn;

extern bool core_init_done;