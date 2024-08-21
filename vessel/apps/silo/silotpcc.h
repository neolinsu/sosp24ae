#pragma once

#ifdef __cplusplus

extern "C"
{

#endif

int silotpcc_init(int number_threads, long numa_memory);
void silotpcc_init_thread(int worker_id);
int silotpcc_exec_one(int worker_id);

#ifdef __cplusplus
}
#endif
