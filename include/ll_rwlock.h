
#ifndef _RTE_RWLOCK_H_
#define _RTE_RWLOCK_H_

/**
 * @file
 *
 *Read-Write Locks
 *
 * This file defines an API for read-write locks. The lock is used to
 * protect data that allows multiple readers in parallel, but only
 * one writer. All readers are blocked until the writer is finished
 * writing.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <errno.h>

#include <ll_common.h>
#include <ll_atomic.h>
#include <ll_pause.h>

/**
 * The ll_rwlock_t type.
 *
 * cnt is -1 when write lock is held, and > 0 when read locks are held.
 */
typedef struct 
{
    volatile int32_t cnt;
} ll_rwlock_t;

/**
 * Initialize the rwlock to an unlocked state.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static __ll_always_inline void
ll_rwlock_init(ll_rwlock_t *rwl){
	rwl->cnt = 0;
}

/**
 * Take a read lock. Loop until the lock is held.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static __ll_always_inline void
ll_rwlock_read_lock(ll_rwlock_t *rwl) {
    int32_t x;
    int success = 0;

    while (success == 0)
    {
        x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
        /* write lock is held */
		if (x < 0) {
			ll_pause();
			continue;
		}
        success = __atomic_compare_exchange_n(&rwl->cnt, &x, x + 1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
    }
}

/**
 * @warning
 *
 * try to take a read lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 * @return
 *   - zero if the lock is successfully taken
 *   - -EBUSY if lock could not be acquired for reading because a
 *     writer holds the lock
 */
static __ll_always_inline int
ll_rwlock_read_trylock(ll_rwlock_t *rwl) {
    int32_t x;
    int success = 0;

    while (success == 0)
    {
        x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
        /* write lock is held */
		if (x < 0) {
            return -EBUSY;
		}
        success = __atomic_compare_exchange_n(&rwl->cnt, &x, x + 1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
    }

    return 0;
}

/**
 * Release a read lock.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static __ll_always_inline void
ll_rwlock_read_unlock(ll_rwlock_t *rwl)
{
	__atomic_fetch_sub(&rwl->cnt, 1, __ATOMIC_RELEASE);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * try to take a write lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 * @return
 *   - zero if the lock is successfully taken
 *   - -EBUSY if lock could not be acquired for writing because
 *     it was already locked for reading or writing
 */
static inline int
ll_rwlock_write_trylock(ll_rwlock_t *rwl)
{
	int32_t x;

	x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
	if (x != 0 || __atomic_compare_exchange_n(&rwl->cnt, &x, -1, 1,
			      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) == 0)
		return -EBUSY;

	return 0;
}

/**
 * Take a write lock. Loop until the lock is held.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
ll_rwlock_write_lock(ll_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
		/* a lock is held */
		if (x != 0) {
			ll_pause();
			continue;
		}
		success = __atomic_compare_exchange_n(&rwl->cnt, &x, -1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
	}
}

/**
 * Release a write lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
ll_rwlock_write_unlock(ll_rwlock_t *rwl)
{
	__atomic_store_n(&rwl->cnt, 0, __ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RWLOCK_H_ */
