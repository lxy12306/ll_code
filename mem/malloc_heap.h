#ifndef _MALLOC_HEAP_H_
#define _MALLOC_HEAP_H_

#include <stdbool.h>
#include <sys/queue.h>

#include <ll_common.h>
#include <ll_spinlock.h>

/* Number of free lists per heap, grouped by size. */
#define LL_HEAP_NUM_FREELISTS  13
#define LL_HEAP_NAME_MAX_LEN 32

struct malloc_elem;

struct malloc_heap {
    ll_spinlock_t lock;
    LIST_HEAD(, malloc_elem) free_head[LL_HEAP_NUM_FREELISTS];
    struct malloc_elem *volatile first;
    struct malloc_elem *volatile last;
    
	unsigned int alloc_count;
	unsigned int socket_id;
	size_t total_size;
	char name[LL_HEAP_NAME_MAX_LEN];
} __ll_cache_aligned;


void *
malloc_heap_alloc(const char *type, size_t size, int socket, unsigned int flags,
		size_t align, size_t bound, bool contig);


#endif //_MALLOC_HEAP_H_