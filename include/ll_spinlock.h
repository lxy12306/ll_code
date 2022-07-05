#ifndef _LL_SPINLOCK_H_
#define _LL_SPINLOCK_H_

#include <ll_common.h>
#include <ll_pause.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The ll_spinlock type
 */
typedef struct {
	volatile int locked; //lock status 0 = unlocked, 1 = locked
} ll_spinlock_t;

/**
 * A static spinlock initializer
 */
#define LL_SPINLOCK_INITIALIZER { 0 }

static __ll_always_inline void
ll_spinlock_init(ll_spinlock_t *sl) {
	sl->locked = 0;
}

static __ll_always_inline void
ll_spinlock_lock(ll_spinlock_t *sl) {
	int exp = 0;
	
	while (!__atomic_compare_exchange_n(&sl->locked, &exp, 1, 0,__ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		ll_wait_until_equal_32((volatile uint32_t *)&sl->locked, 0, __ATOMIC_RELAXED);
		exp = 0;
	}
}


static __ll_always_inline void
ll_spinlock_unlock(ll_spinlock_t *sl) {
	__atomic_store_n(&sl->locked, 0, __ATOMIC_RELEASE);
}

static __ll_always_inline int 
ll_spinlock_trylock(ll_spinlock_t *sl) {
	int exp = 0;
	return __atomic_compare_exchange_n(&sl->locked, &exp, 1, 0,
				__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

static __ll_always_inline int
ll_spinlock_is_locked (ll_spinlock_t *sl) {
	return __atomic_load_n(&sl->locked, __ATOMIC_ACQUIRE);
}

#ifdef __cplusplus
}
#endif

#endif //_LL_SPINLOCK_H_
