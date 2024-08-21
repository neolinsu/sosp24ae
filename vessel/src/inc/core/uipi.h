#pragma once
#include <scheds/state.h>

typedef void(*op_func)(void);

struct uipi_ops {
   op_func  to_cede;
   op_func  to_yield;
};

int register_uipi_handlers(void);

extern __thread struct core_conn *perc_core_conn;
