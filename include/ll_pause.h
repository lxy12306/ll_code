#ifndef _LL_PAUSE_H_
#define _LL_PAUSE_H_

#include <assert.h>
#include <emmintrin.h>
#include <ll_common.h>

static inline void ll_pause(void) {
	_mm_pause();
}

static __ll_always_inline void
ll_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
	int memorder) {
	assert(memorder == __ATOMIC_ACQUIRE || memorder == __ATOMIC_RELAXED);
	
	while (__atomic_load_n(addr, memorder) != expected)
		ll_pause();
}
#endif
