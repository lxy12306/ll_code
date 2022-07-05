	.text
	.globl	ll_memset_short_64
	.type	ll_memset_short_64, @function
ll_memset_short_64:
	movq	%rdi, -8(%rsp)
	movl	%esi, %eax
	movq	%rdx, %rcx

	rep stosb

	movq	-8(%rsp), %rax
	ret
	.size	ll_memset_short_64, .-ll_memset_short_64
