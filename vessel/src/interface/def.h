#pragma once

const uint64_t PKU_CPU_PTR = VESSEL_MEM_GLOBAL_START + align_up(
                                    sizeof(vessel_mem_meta_t) +
                                    sizeof(cpu_state_map_t) +
                                    sizeof(task_map_t) +
                                    sizeof(kthread_meta_map_t)
                                );
