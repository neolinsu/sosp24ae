#include "vcontext/vcontext_asm.h"
	.text
    .align 16
	.globl	init_switch_to
	.type	init_switch_to, @function
init_switch_to:
    movq OFF_RBP(%rdi), %rbp
    movq OFF_R12(%rdi), %r12
	movq OFF_R13(%rdi), %r13
	movq OFF_R14(%rdi), %r14
	movq OFF_R15(%rdi), %r15

    movq OFF_RSI(%rdi), %rsi
    movq OFF_RDX(%rdi), %rdx
    movq OFF_RCX(%rdi), %rcx
    movq OFF_R8 (%rdi), %r8
    movq OFF_R9 (%rdi), %r9

    movq OFF_FS(%rdi), %rbx
    wrfsbase %rbx
    movq OFF_GS(%rdi), %rbx
    //wrgsbase %rbx

    fldenv OFF_fpstate(%rdi)
    ldmxcsr OFF_xmstate(%rdi)

    movq OFF_RBX(%rdi), %rbx
    push OFF_RIP(%rdi)
    movq OFF_RDI(%rdi), %rdi
    ret
