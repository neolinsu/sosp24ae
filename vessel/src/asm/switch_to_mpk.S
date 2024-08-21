#include "vcontext/vcontext_asm.h"
.text
    .align 16
	.globl	__switch_to_user_via_ret
	.type	__switch_to_user_via_ret, @function
__switch_to_user_via_ret:

    movq OFF_RBP(%rdi), %rbp
    movq OFF_R12(%rdi), %r12
	movq OFF_R13(%rdi), %r13
	movq OFF_R14(%rdi), %r14
	movq OFF_R15(%rdi), %r15

    movq OFF_RSI(%rdi), %rsi
    // movq OFF_RDX(%rdi), %rdx // push
    // movq OFF_RCX(%rdi), %rcx // push
    movq OFF_R8 (%rdi), %r8
    movq OFF_R9 (%rdi), %r9

    // movq OFF_RAX (%rdi), %rax // push
    movq OFF_R11 (%rdi), %r11
    movq OFF_R10 (%rdi), %r10

    movq OFF_FS(%rdi), %rbx
    wrfsbase %rbx
    movq OFF_GS(%rdi), %rbx
    //wrgsbase %rbx

    //fldenv OFF_fpstate(%rdi)
    //ldmxcsr OFF_xmstate(%rdi)

__switch_to_user_via_ret_safe_redo:
    xor %rcx, %rcx
    rdtscp
    andq $0xFFF, %rcx
    movabsq $0x8d06a000, %rax
    mov (%rax, %rcx, 4), %eax
    xor %ecx, %ecx
    xor %edx, %edx
    WRPKRU
    xor %rcx, %rcx
    rdtscp
    andq $0xFFF, %rcx
    movabsq $0x8d06a000, %rax
    mov (%rax, %rcx, 4), %ebx
    xor %ecx, %ecx
    RDPKRU
    cmp %ebx, %eax
    jne __switch_to_user_via_ret_safe_redo

    movq OFF_RDX(%rdi), %rdx
    movq OFF_RCX(%rdi), %rcx
    movq OFF_RBX(%rdi), %rbx
    movq OFF_RAX(%rdi), %rax
    movq OFF_RSP(%rdi), %rsp
    pushq OFF_RIP(%rdi)
    movq OFF_RDI(%rdi), %rdi
    ret
    nop

.text
    .align 16
	.globl	__switch_to_user_via_ret_wo_fs
	.type	__switch_to_user_via_ret_wo_fs, @function
__switch_to_user_via_ret_wo_fs:

  	/* restore callee regs */
	movq    OFF_RBX(%rdi), %rbx
	movq    OFF_RBP(%rdi), %rbp
	movq    OFF_R12(%rdi), %r12
	movq    OFF_R13(%rdi), %r13
	movq    OFF_R14(%rdi), %r14
	movq    OFF_R15(%rdi), %r15

    //fldenv OFF_fpstate(%rdi)
    //ldmxcsr OFF_xmstate(%rdi)

	/* restore ip and stack */
	movq    OFF_RSP(%rdi), %rsp
	movq    OFF_RIP(%rdi), %rsi

    jmpq	*%rsi
	nop