#ifndef _LL_PER_THREAD_H
#define _LL_PER_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#define LL_DEFINE_PER_THREAD(type, name)			\
	__thread __typeof__(type) per_thread_##name

#define LL_DECLARE_PER_THREAD(type, name)			\
	extern __thread __typeof__(type) per_thread_##name

#define LL_PER_THREAD(name) (per_thread_##name)


#define LL_DEFINE_PER_LCORE(type,name)	\
	__thread __typeof__(type) per_lcore_##name

#define LL_DECLARE_PER_LCORE(type,name)	\
	extern __thread __typeof__(type) per_lcore_##name

#define LL_PER_LCORE(name) (per_lcore_##name)

#define CPU_SETSIZE 8
#define _BITS_PER_SET (sizeof(long long) * 8)
#define _BIT_SET_MASK (_BITS_PER_SET - 1)

#define _NUM_SETS(b) (((b) + _BIT_SET_MASK) / _BITS_PER_SET)
#define _WHICH_SET(b) ((b) / _BITS_PER_SET)
#define _WHICH_BIT(b) ((b) & (_BITS_PER_SET - 1))

struct ll_cpuset_t {
	long long _bits[_NUM_SETS(CPU_SETSIZE)];
};

#define CPU_SET(b, s) ((s)->_bits[_WHICH_SET(b)] |= (1LL << _WHICH_BIT(b)))

#define CPU_ZERO(s)							\
	do {								\
		unsigned int _i;					\
									\
		for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++)		\
			(s)->_bits[_i] = 0LL;				\
	} while (0)

/**
 * Structure storing internal configuration (per-lcore)
 */
struct lcore_config {

    unsigned int socket_id; /**< physical socket id for this lcore */
    unsigned int core_id; /**< core number on socket for this lcore */
    int core_index;            /**< relative index, starting from 0 */
	unsigned char core_role; /**< role of core eg: OFF, LL, SERVICE */

	struct ll_cpuset_t cpuset;
};

/**
 * The lcore role (used in LL or not).
 */
enum ll_lcore_role_t {
	ROLE_LL,
	ROLE_OFF,
	ROLE_SERVICE,
	ROLE_NON_EAL,
};


#ifdef __cplusplus
}
#endif

#endif /*_LL_PER_THREAD_H*/
