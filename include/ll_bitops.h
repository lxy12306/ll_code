#ifndef _LL_BITOPS_H_
#define _LL_BITOPS_H_

#include <stdint.h>

#ifdef _cplusplus
extern "C" {
#endif


/**
 * Get the uint64_t value for a specified bit set.
 *
 * @param nr
 *   The bit number in range of 0 to 63.
 */
#define LL_BIT64(nr) (UINT64_C(1) << (nr))

/**
 * Get the uint32_t value for a specified bit set.
 *
 * @param nr
 *   The bit number in range of 0 to 31.
 */
#define LL_BIT32(nr) (UINT32_C(1) << (nr))


#ifdef _cplusplus
}
#endif

#endif