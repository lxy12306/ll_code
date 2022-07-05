#ifndef _LL_COMMON_H_
#define _LL_COMMON_H_

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef typeof
#define typeof __typeof__
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#define LL_STD_C11 __extension__
#else
#define LL_STD_C11
#endif

#define LL_PATH_MAX 256


/**
 * Force alignment
 */
#define __ll_aligned(a) __attribute__((__aligned__(a)))

#define LL_CACHE_LINE_MIN_SIZE 64

//force alignment to cache line
#define __ll_cache_aligned __ll_aligned(LL_CACHE_LINE_MIN_SIZE)

/**
 * 结构体取消�?�齐，紧凑排�?
 * */
#define __ll_packed __attribute__((__packed__))


#ifdef LL_ARCH_STRICT_ALIGN
#else
typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;
#endif 


/*以下为类型别�?*/
typedef uint64_t ll_phys_addr_t; //物理地址
#define LL_BAD_PHYS_ADDR (ll_phys_addr_t(-1))

typedef uint64_t ll_iova_t; //IO虚拟地址
#define LL_BAD_IOVA ((ll_iova_t)(-1))


//常用宏定�?

#define LL_CACHE_LINE_SIZE 64
#define LL_CACHE_LINE_MASK (LL_CACHE_LINE_SIZE - 1)

/**
* Macro to align a value to a given power - of - two.The resultant value
* will be of the same type as the first parameter, and will be no
* bigger than the first parameter.Second parameter must be a
* power - of - two value.
* 向上对齐�?2的n次幂运算  align必须�?2的n次幂
*/
#define LL_ALIGN_FLOOR(val, align) \
	(typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 * 向下对齐�?2的n次幂运算
 */
#define LL_ALIGN_CEIL(val, align) \
	LL_ALIGN_FLOOR(((val) + ((typeof(val)) (align) - 1)), align)

#define LL_PRIORITY_LOG 101
#define LL_PRIORITY_CFG 1001
#define LL_PRIORITY_MCFG 1002
#define LL_PRIORITY_LAST 65535

#define LL_PRIO(prio) \
	LL_PRIORITY_ ## prio

#ifndef LL_INIT_PRIO /* Allow to override from EAL */
#define LL_INIT_PRIO(func, prio) \
static void __attribute__((constructor(LL_PRIO(prio)), used)) func(void)
#endif

#define LL_INIT(func) \
	LL_INIT_PRIO(func, LAST)


/** Number of elements in the array. */
#define	LL_DIM(a)	(sizeof (a) / sizeof ((a)[0]))


 /**
  * Macro to align a value to a given power-of-two. The resultant
  * value will be of the same type as the first parameter, and
  * will be no lower than the first parameter. Second parameter
  * must be a power-of-two value.
  * This function is the same as LL_ALIGN_CEIL
  * 向下对齐�?2的n次幂运算
  */
#define LL_ALIGN(val, align) LL_ALIGN_CEIL(val, align)


//常用函数


/***************************************
 * function attribute 
 ***************************************/

/**
 * Hint never returing function
 */
#define __ll_noreturn __attribute__((noreturn))

/**
 * Force a function to be inlined
 */
#define __ll_always_inline inline __attribute__((always_inline))

/**
 * Force a function to be noinlined
 */
#define __ll_noinline __attribute__((noinline))

/**
 * Hint funciton in the hot path
 */
#define __ll_hot __attribute__((hot))

/**
 * Hint funciton in the cold path
 */
#define __ll_cold __attribute__((cold))

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#define __ll_unused __attribute__((__unused__))


/**
 * Mark pointer as restricted with regard to pointer aliasing.
 */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#define __ll_restrict __restrict
#else
#define __ll_restrict restrict
#endif


/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define LL_SET_USED(x) (void)(x)


/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int
ll_is_power_of_2(uint32_t n)
{
	return n && !(n & (n - 1));
}

/**
 * Combines 32b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param x
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint32_t
ll_combine32ms1b(uint32_t x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return x;
}

/**
 * Combines 64b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param v
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint64_t
ll_combine64ms1b(uint64_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	return v;
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint32_t
ll_align32pow2(uint32_t x)
{
	x--;
	x = ll_combine32ms1b(x);

	return x + 1;
}

/*********** Other general functions / macros ********/

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
ll_bsf32(uint32_t v)
{
	return (uint32_t)__builtin_ctz(v);
}

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define LL_MIN(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a < _b ? _a : _b; \
	})

/**
 * Macro to return the maximum of two numbers
 */
#define LL_MAX(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a > _b ? _a : _b; \
	})
/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset to a pointer
 */
#define LL_PTR_ADD(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define LL_PTR_SUB(ptr, x) ((void*)((uintptr_t)ptr - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define LL_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/*********** Macros for pointer alignment ********/

#define LL_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))LL_ALIGN_FLOOR((uintptr_t)ptr, align))

#define LL_PTR_ALIGN_CEIL(ptr, align) \
	LL_PTR_ALIGN_FLOOR((typeof(ptr))LL_PTR_ADD((ptr), (align) - 1), align)

#define LL_PTR_ALIGN(ptr, align) LL_PTR_ALIGN_CEIL(ptr, align)

/****** Macros for compile type checks ******/
//triggers an error at compilation time if the condition if true
#define LL_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

/****** Memory flags ******/

/** Memory protection flags. */
enum ll_mem_prot {
	LL_PROT_READ = 1 << 0,   /**< Read access. */
	LL_PROT_WRITE = 1 << 1,  /**< Write access. */
	LL_PROT_EXECUTE = 1 << 2 /**< Code execution. */
};

/** Additional flags for memory mapping. */
enum ll_map_flags {
	/** Changes to the mapped memory are visible to other processes. */
	LL_MAP_SHARED = 1 << 0,
	/** Mapping is not backed by a regular file. */
	LL_MAP_ANONYMOUS = 1 << 1,
	/** Copy-on-write mapping, changes are invisible to other processes. */
	LL_MAP_PRIVATE = 1 << 2,
	/**
	 * Force mapping to the requested address. This flag should be used
	 * with caution, because to fulfill the request implementation
	 * may remove all other mappings in the requested region. However,
	 * it is not required to do so, thus mapping with this flag may fail.
	 */
	LL_MAP_FORCE_ADDRESS = 1 << 3
};

#endif //_LL_COMMON_H_
