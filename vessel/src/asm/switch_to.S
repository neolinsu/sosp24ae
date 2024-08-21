#include "vcontext/vcontext_asm.h"
.text
    .align 16
	.globl	switch_to
	.type	switch_to, @function
switch_to:
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

    movq OFF_RAX (%rdi), %rax
    movq OFF_R11 (%rdi), %r11
    movq OFF_R10 (%rdi), %r10

    //fldenv OFF_fpstate(%rdi)
    //ldmxcsr OFF_xmstate(%rdi)

    movq OFF_UINTR_RSP(%rdi), %rsp
    popq %rbx
    movq OFF_RBX(%rdi), %rbx
    movq OFF_RDI(%rdi), %rdi
    uiret