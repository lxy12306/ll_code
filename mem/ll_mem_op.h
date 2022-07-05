#ifndef _LL_MEM_OP_H_
#define _LL_MEM_OP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/wordsize.h>

#if __WORDSIZE == 64

#define ll_memset ll_memset64

#else
#endif


#ifdef __cplusplus
}
#endif

#endif

