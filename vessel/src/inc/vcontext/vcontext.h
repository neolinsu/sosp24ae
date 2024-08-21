#pragma once
#include <stdint.h>
struct vcontext {
	uint32_t xmstate;
	uint32_t fpstate[7];

	unsigned long long FS;
	unsigned long long GS;

	unsigned long long R10;
	unsigned long long R11;
	unsigned long long RAX;

	unsigned long long R9 ;
	unsigned long long R8 ;
	unsigned long long RCX;
	unsigned long long RDX;
	unsigned long long RSI;
	unsigned long long RDI;

	unsigned long long R15;
	unsigned long long R14;
	unsigned long long R13;
	unsigned long long R12;
	unsigned long long RBP;
	unsigned long long RBX;

	unsigned long long UINTR_RSP;

	unsigned long long vector;
	unsigned long long RIP;
	unsigned long long RF;
	unsigned long long RSP;
};

typedef struct vcontext vcontext_t;
