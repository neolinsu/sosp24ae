#include "vcontext/vcontext_asm.h"

	.text
	.align 16
	.globl	__save_jmp
	.type	__save_jmp, @function
__save_jmp:
	movq %rbx, OFF_RBX(%rdi)
    movq %rbp, OFF_RBP(%rdi)
    movq %r12, OFF_R12(%rdi)
	movq %r13, OFF_R13(%rdi)
	movq %r14, OFF_R14(%rdi)
	movq %r15, OFF_R15(%rdi)

	// fnstenv   OFF_fpstate(%rdi)
	// stmxcsr   OFF_xmstate(%rdi)

    movq    (%rsp), %r8
	movq    %r8, OFF_RIP(%rdi)
	leaq    8(%rsp), %r8
	movq    %r8, OFF_RSP(%rdi)

	movq    %rdx, %rsp

	jmpq    *%rsi
	nop


