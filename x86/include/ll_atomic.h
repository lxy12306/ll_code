#ifndef _LL_ATOMIC_X86_H_
#define _LL_ATOMIC_X86_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <ll_common.h>


#include <emmintrin.h>
#include "../../include/ll_atomic.h"

#define MPLOCKED  "lock ;" /*Insert MP lock prefix*/


#define ll_mb() _mm_mfence()

#define ll_wmb() _mm_sfence()

#define ll_rmb() _mm_lfence()

#define ll_smp_wmb() ll_compiler_barrier()

#define ll_smp_rmb() ll_compiler_barrier()
	
static __ll_always_inline void
ll_smp_mb(void) 
{
	asm volatile("lock addl $0, -128(%%rsp); " ::: "memory");
}

/*------------------------------------32 bit atomic operations ------------------------------------*/
static inline int
ll_atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src) {
	uint8_t res;
	asm volatile(
			MPLOCKED
			"cmpxchgl %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res), /*output*/
			  [dst] "=m" (*dst)
			: [src] "r" (src), /* input */
			 "a" (exp),
			 "m" (*dst)
			: "memory"); /* no-clobber list*/
	return res;
}


static inline void
ll_atomic32_set(volatile uint32_t *dst, uint32_t src) {
	asm volatile(
		MPLOCKED
		"movl %[src], %[dst];"
		: [dst] "=m" (*dst)
		: [src] "r" (src),
		 "m" (*dst)
		: "memory");
}


static inline uint32_t
ll_atomic32_exchange(volatile uint32_t *dst, uint32_t val) {
	asm volatile(
			MPLOCKED
			"xchgl %0, %1;"
			: "=r"(val), "=m" (*dst)
			: "0" (val), "m"(*dst)
			: "memory"); /* no-clobber list*/
	return val;
}

#ifdef __cplusplus
}
#endif
#endif
