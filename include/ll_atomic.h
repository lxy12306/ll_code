#ifndef _LL_ATOMIC_H
#define _LL_ATOMIC_H

#include "../x86/include/ll_atomic.h"
#define LL_ARCH_64 1

#if LL_ARCH_64
	#include "../x86/include/ll_atomic_64.h"
#endif


/**
 * Compiler barrier
 * 
 * Guarantees that operation reordering does not occur at compile time
 * for operation directly before and after the barrier.
 */
#define ll_compiler_barrier() do {	  \
	asm volatile ("" : : : "memory"); \
} while(0)

#endif
