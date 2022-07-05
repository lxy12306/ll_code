#ifndef _LL_DEBUG_H_
#define _LL_DEBUG_H_

#ifdef LL_ENABLE_ASSERT
#define LL_ASSERT(exp) do {} while (0)
#else
#define LL_ASSERT(exp) do {} while (0)
#endif

#define ll_panic(...) ll_panic_(__func__, __VA_ARGS__, "dummy")
#define ll_panic_(func, format, ...) __ll_panic(func, format "%.0s", __VA_ARGS__)

#define LL_VERIFY(exp) do { \
    if (unlikely(!(exp)))   \
        ll_panic("line %d\tassert \"%s\" failed\n", __LINE__, #exp); \
} while(0);

/*
 * Provide notification of a critical non-recoverable error and stop.
 *
 * This function should not be called directly. Refer to rte_panic() macro
 * documentation.
 */
void __ll_panic(const char *funcname , const char *format, ...);

#endif
