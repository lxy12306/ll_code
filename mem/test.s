	.file	"test.cpp"
	.text
	.section	.rodata
.LC0:
	.string	"%f "
	.text
	.globl	_Z5printPfi
	.type	_Z5printPfi, @function
_Z5printPfi:
.LFB4015:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movl	%esi, -28(%rbp)
	movl	$0, -4(%rbp)
.L3:
	movl	-4(%rbp), %eax
	cmpl	-28(%rbp), %eax
	jge	.L2
	movl	-4(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-24(%rbp), %rax
	addq	%rdx, %rax
	movss	(%rax), %xmm0
	cvtss2sd	%xmm0, %xmm0
	leaq	.LC0(%rip), %rdi
	movl	$1, %eax
	call	printf@PLT
	addl	$1, -4(%rbp)
	jmp	.L3
.L2:
	movl	$10, %edi
	call	putchar@PLT
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE4015:
	.size	_Z5printPfi, .-_Z5printPfi
	.globl	_Z18test_mm_insert_epiv
	.type	_Z18test_mm_insert_epiv, @function
_Z18test_mm_insert_epiv:
.LFB4016:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$64, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	movl	$128, -64(%rbp)
	movl	$4, -60(%rbp)
	movl	-60(%rbp), %eax
	cltq
	salq	$2, %rax
	movq	%rax, %rsi
	movl	$64, %edi
	call	aligned_alloc@PLT
	movq	%rax, -56(%rbp)
	movdqa	-32(%rbp), %xmm0
	movl	$10, %eax
	pinsrb	$1, %eax, %xmm0
	movaps	%xmm0, -48(%rbp)
	movl	-60(%rbp), %eax
	sall	$2, %eax
	movslq	%eax, %rdx
	leaq	-48(%rbp), %rcx
	movq	-56(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	memcpy@PLT
	nop
	movq	-8(%rbp), %rax
	xorq	%fs:40, %rax
	je	.L5
	call	__stack_chk_fail@PLT
.L5:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE4016:
	.size	_Z18test_mm_insert_epiv, .-_Z18test_mm_insert_epiv
	.globl	main
	.type	main, @function
main:
.LFB4017:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	$0, %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE4017:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	 1f - 0f
	.long	 4f - 1f
	.long	 5
0:
	.string	 "GNU"
1:
	.align 8
	.long	 0xc0000002
	.long	 3f - 2f
2:
	.long	 0x3
3:
	.align 8
4:
