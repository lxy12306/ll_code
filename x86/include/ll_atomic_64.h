
#ifndef _LL_ATOMIC_64_H_
#define _LL_ATOMIC_64_H_


/*------------------------- 64 bit atomic operations -------------------------*/

static inline void
ll_atomic64_init(volatile int64_t *v)
{
	*v = 0;
}

static inline int64_t
ll_atomic64_read(volatile int64_t *v)
{
	return *v;
}


static inline void
ll_atomic64_set(volatile int64_t *v, int64_t new_value)
{
	*v = new_value;
}


#endif //_LL_ATOMIC_64_H_