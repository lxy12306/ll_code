#ifndef	_LL_PREFETCH_X86_64_H_
#define _LL_PREFETCH_X86_64_H_

#ifdef __cplusplus
extern "C" {
#endif

static inline void ll_prefetch0(const volatile void *p) {
	asm volatile ("prefetcht0 %[p]" : : [p] "m" (*(const volatile char *)p));
}

#ifdef __cplusplus
}
#endif
