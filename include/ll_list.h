#ifndef _LL_LIST_H_
#define _LL_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

#define LL_TAILQ_HEAD(name, type) \
struct name {	\
	struct type *tqh_first; \
	struct type **tqh_last; \
}

#define LL_TAILQ_ENTRY(type) \
struct { \
	struct type *tqe_next; \
	struct type **tqe_prev; \
}

#define LL_TAILQ_FIRST(head) ((head)->tqh_first)
#define LL_TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)

#define LL_TAILQ_FOREACH(var, head, field) \
	for ((var) = LL_TAILQ_FIRST((head));	\
		(var);	\
		(var) = LL_TAILQ_NEXT((var), (field)))

#define LL_STAILQ_HEAD(name, type) \
struct name {	\
	struct type *stqh_first;	\
	struct type **stqh_last;	\
}
#define LL_STAILQ_ENTRY(type)	\
struct {	\
	struct type *stqe_next;	\
}
	
#ifdef __cplusplus
}
#endif

#endif //_LL_LIST_H_
