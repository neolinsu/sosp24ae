/*
* APIS to export to Runtime.
*/
#pragma once
#include <stddef.h>

#include "core/task.h"

extern task_id_t vessel_tid();

extern int vessel_api_alloc_chunk(void *args);
extern void* vessel_api_kthread_park(void *args);


int api_init(void);