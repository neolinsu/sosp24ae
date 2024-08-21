/*
 * switch.S - assembly routines for switching trap frames
 */

/*
 * Trap Frame Format
 * WARNING: These values reflect the layout of struct thread_tf. Don't change
 * these values without also updating defs.h.
 */
#include "vcontext/vcontext_asm.h"

.file "switch.S"
.text


/**
 * __jmp_thread - executes a thread from the runtime
 * @tf: the trap frame to restore (%rdi)
 *
 * This low-level variant isn't intended to be called directly.
 * Re-enables preemption, parking the kthread if necessary.
 * Does not return.
 */
.align 16
.globl __jmp_thread
.type __jmp_thread, @function
__jmp_thread:
	/* restore callee regs */
	movq    OFF_RBX(%rdi), %rbx
	movq    OFF_RBP(%rdi), %rbp
	movq    OFF_R12(%rdi), %r12
	movq    OFF_R13(%rdi), %r13
	movq    OFF_R14(%rdi), %r14
	movq    OFF_R15(%rdi), %r15

	/* restore ip and stack */
	movq    OFF_RSP(%rdi), %rsp
	//pushq OFF_RSP(%rdi)
	//pushq OFF_RF(%rdi)
	//pushq OFF_RIP(%rdi)
	movq    OFF_RIP(%rdi), %rsi
	
	movq    OFF_RDI(%rdi), %rdi /* ARG0 */

	/* re-enable preemption */
	subl	$1, %fs:preempt_cnt@tpoff
	jz	1f

	/* jump into trap frame */
	//uiret
#ifdef VESSEL_UIPI
	stui
#endif
	jmpq	*%rsi
	nop

1:	/* cold-path, save RIP and park the kthread */
	pushq	%rsi
	pushq	%rdi
	pushq	%r15
	movq	%rsp, %r15
	andq	$0xfffffffffffffff0, %rsp /* ensure correct stack alignment */
	call	preempt
	movq	%r15, %rsp /* restore SP */
	popq	%r15
	popq	%rdi
	popq	%rsi
	//uiret
	jmpq	*%rsi

/**
 * __jmp_thread_hard_ret - executes a thread from the runtime
 * @tf: the trap frame to restore (%rdi)
 *
 * This low-level variant isn't intended to be called directly.
 * Re-enables preemption, parking the kthread if necessary.
 * Does not return.
 */
.align 16
.globl __jmp_thread_hard_ret
.type __jmp_thread_hard_ret, @function
__jmp_thread_hard_ret:
	/* CPU give up handle uipi in hard. */
	/* restore callee regs */
	movq    OFF_RBX(%rdi), %rbx
	movq    OFF_RBP(%rdi), %rbp
	movq    OFF_R12(%rdi), %r12
	movq    OFF_R13(%rdi), %r13
	movq    OFF_R14(%rdi), %r14
	movq    OFF_R15(%rdi), %r15

	/* restore ip and stack */
	movq    OFF_RSP(%rdi), %rsp
	movq    OFF_RIP(%rdi), %rsi
	
	movq    OFF_RDI(%rdi), %rdi /* ARG0 */

	/* re-enable preemption */
	subl	$1, %fs:preempt_cnt@tpoff
	jz	1f

	/* jump into trap frame */
	jmpq	*%rsi
	nop

1:	/* cold-path, save RIP and park the kthread */
	pushq	%rsi
	pushq	%rdi
	pushq	%r15
	movq	%rsp, %r15
	andq	$0xfffffffffffffff0, %rsp /* ensure correct stack alignment */
	call	preempt_hard
	movq	%r15, %rsp /* restore SP */
	popq	%r15
	popq	%rdi
	popq	%rsi
	jmpq	*%rsi

/**
 * __jmp_thread_direct - directly switches from one thread to the next
 * @oldtf: the trap frame to save (%rdi)
 * @newtf: the trap frame to restore (%rsi)
 * @thread_running: a pointer to whether the thread is still running (%rdx)
 *
 * This low-level variant isn't intended to be called directly.
 * Re-enables preemption, parking the kthread if necessary.
 * Does return.
 */
.align 16
.globl __jmp_thread_direct
.type __jmp_thread_direct, @function
__jmp_thread_direct:
	/* save ip and stack */
	movq    (%rsp), %r8
	movq    %r8, OFF_RIP(%rdi)
	leaq    8(%rsp), %r8
	movq    %r8, OFF_RSP(%rdi)

	/* save callee regs */
	movq    %rbx, OFF_RBX(%rdi)
	movq    %rbp, OFF_RBP(%rdi)
	movq    %r12, OFF_R12(%rdi)
	movq    %r13, OFF_R13(%rdi)
	movq    %r14, OFF_R14(%rdi)
	movq    %r15, OFF_R15(%rdi)

	/* restore ip and stack */
	movq    OFF_RSP(%rsi), %rsp
	movq    OFF_RIP(%rsi), %rcx

	/* clear the stack busy flag */
	movl	$0, (%rdx)

	/* restore callee regs */
	movq    OFF_RBX(%rsi), %rbx
	movq    OFF_RBP(%rsi), %rbp
	movq    OFF_R12(%rsi), %r12
	movq    OFF_R13(%rsi), %r13
	movq    OFF_R14(%rsi), %r14
	movq    OFF_R15(%rsi), %r15

	/* set first argument (in case new thread) */
	movq    OFF_RDI(%rsi), %rdi /* ARG0 */

	/* re-enable preemption */
	subl	$1, %fs:preempt_cnt@tpoff
	jz	1f

	/* jump into trap frame */
	jmpq	*%rcx
	nop

1:	/* cold-path, save RIP and park the kthread */
	pushq	%rcx
	pushq	%rdi
	pushq	%r15
	movq	%rsp, %r15
	andq	$0xfffffffffffffff0, %rsp /* ensure correct stack alignment */
	call	preempt
	movq	%r15, %rsp /* restore SP */
	popq	%r15
	popq	%rdi
	popq	%rcx
	jmpq	*%rcx

/**
 * __jmp_thread_direct_hard_ret - directly switches from one thread to the next
 * @oldtf: the trap frame to save (%rdi)
 * @newtf: the trap frame to restore (%rsi)
 * @thread_running: a pointer to whether the thread is still running (%rdx)
 *
 * This low-level variant isn't intended to be called directly.
 * Re-enables preemption, parking the kthread if necessary.
 * Does return.
 */
.align 16
.globl __jmp_thread_direct_hard_ret
.type __jmp_thread_direct_hard_ret, @function
__jmp_thread_direct_hard_ret:
	/* save ip and stack */
	movq    (%rsp), %r8
	movq    %r8, OFF_RIP(%rdi)
	leaq    8(%rsp), %r8
	movq    %r8, OFF_RSP(%rdi)

	/* save callee regs */
	movq    %rbx, OFF_RBX(%rdi)
	movq    %rbp, OFF_RBP(%rdi)
	movq    %r12, OFF_R12(%rdi)
	movq    %r13, OFF_R13(%rdi)
	movq    %r14, OFF_R14(%rdi)
	movq    %r15, OFF_R15(%rdi)

	/* restore ip and stack */
	movq    OFF_RSP(%rsi), %rsp
	movq    OFF_RIP(%rsi), %rcx

	/* clear the stack busy flag */
	movl	$0, (%rdx)

	/* restore callee regs */
	movq    OFF_RBX(%rsi), %rbx
	movq    OFF_RBP(%rsi), %rbp
	movq    OFF_R12(%rsi), %r12
	movq    OFF_R13(%rsi), %r13
	movq    OFF_R14(%rsi), %r14
	movq    OFF_R15(%rsi), %r15

	/* set first argument (in case new thread) */
	movq    OFF_RDI(%rsi), %rdi /* ARG0 */

	/* re-enable preemption */
	subl	$1, %fs:preempt_cnt@tpoff
	jz	1f

	/* jump into trap frame */
	jmpq	*%rcx
	nop

1:	/* cold-path, save RIP and park the kthread */
	pushq	%rcx
	pushq	%rdi
	pushq	%r15
	movq	%rsp, %r15
	andq	$0xfffffffffffffff0, %rsp /* ensure correct stack alignment */
	call	preempt_hard
	movq	%r15, %rsp /* restore SP */
	popq	%r15
	popq	%rdi
	popq	%rcx
	jmpq	*%rcx

/**
 * __jmp_runtime - saves the current trap frame and jumps to a function in the
 *                 runtime
 * @tf: the struct thread_tf to save state (%rdi)
 * @fn: the function pointer to call (%rsi)
 * @stack: the start of the runtime stack (%rdx)
 *
 * This low-level variant isn't intended to be called directly.
 * Must be called with preemption disabled.
 * No return value.
 */
.align 16
.globl __jmp_runtime
.type __jmp_runtime, @function
__jmp_runtime:
	/* save callee regs */
	movq    %rbx, OFF_RBX(%rdi)
	movq    %rbp, OFF_RBP(%rdi)
	movq    %r12, OFF_R12(%rdi)
	movq    %r13, OFF_R13(%rdi)
	movq    %r14, OFF_R14(%rdi)
	movq    %r15, OFF_R15(%rdi)

	/* save ip and stack */
	movq    (%rsp), %r8
	movq    %r8, OFF_RIP(%rdi)
	leaq    8(%rsp), %r8
	movq    %r8, OFF_RSP(%rdi)

	/* jump into runtime function */
	movq    %rdx, %rsp

	/* jump into runtime code */
	jmpq    *%rsi

/**
 * __jmp_runtime_nosave - jumps to a function in the runtime without saving the
 *			  current stack frame
 * @fn: the function pointer to call (%rdi)
 * @stack: the start of the runtime stack (%rsi)
 *
 * This low-level variant isn't intended to be called directly.
 * Must be called with preemption disabled.
 * No return value.
 */
.align 16
.globl __jmp_runtime_nosave
.type __jmp_runtime_nosave, @function
__jmp_runtime_nosave:

	/* jump into runtime function */
	movq    %rsi, %rsp
	movq	%rdi, %rsi

	/* jump into runtime code */
	jmpq    *%rsi
