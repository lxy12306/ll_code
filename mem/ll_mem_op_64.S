	.text
	.globl	ll_memset_short_64
	.type	ll_memset_short_64, @function
ll_memset_short_64:
	endbr64
	pushq	%rbp
	movq	%rsp, %rbp
	pushq	%rcx
	movq	%rdi, -8(%rbp)
	movl	%esi, %eax
	movq	%rdx, %rcx

	rep stosb

	movq	-8(%rbp), %rax
	popq	%rcx
	popq	%rbp
	ret
	.size	ll_memset_short_64, .-ll_memset_short_64
	.globl	ll_memset_aligned_64_sse
	.type	ll_memset_aligned_64_sse, @function
ll_memset_aligned_64_sse:
	endbr64
	mov %esi,%eax
	pushq	%rbp
	mov %al,%ah
	movq	%rsp, %rbp
	
	movd  %eax, %xmm0
	pshuflw $16, %xmm0, %xmm1
	pshufd $0, %xmm1, %xmm0




	popq	%rbp
	ret
	.size ll_memset_aligned_64_sse, .-ll_memset_aligned_64_sse
