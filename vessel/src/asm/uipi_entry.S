	.text
    .align 16
	.globl	uipi_entry
	.type	uipi_entry, @function
uipi_entry:
    pushq   %rsp
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
	pushq   %r13
	pushq   %r14
	pushq   %r15

    pushq   %rdi
    pushq   %rsi
    pushq   %rdx
    pushq   %rcx
    pushq   %r8
    pushq   %r9

    pushq %rax
    pushq %r11
    pushq %r10

    //rdgsbase    %rcx
    pushq		$0
    rdfsbase    %rcx
    pushq		%rcx

    subq $28, %rsp
    //fnstenv   (%rsp)
    subq $4, %rsp
    //stmxcsr  (%rsp)

    movq %rsp, %rdi
	andq	$0xfffffffffffffff0, %rsp /* ensure correct stack alignment */
    call uipi_handler

