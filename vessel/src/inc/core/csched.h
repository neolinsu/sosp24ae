#pragma once

void cluster_yield();
void cluster_exit() __noreturn;
void cluster_mwait(void);
int  cluster_id(void);