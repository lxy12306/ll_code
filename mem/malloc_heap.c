#include <stdio.h>

#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_log.h>
#include <ll_memzone.h>
#include <ll_memory.h>

#include "malloc_elem.h"

/* start external socket ID's at a very high number */
#define CONST_MAX(a, b) (a > b ? a : b) /* LL_MAX is not a constant */
#define EXTERNAL_HEAP_MIN_SOCKET_ID (CONST_MAX((1 << 8), LL_MAX_NUMA_NODES))

static unsigned
check_hugepage_sz(unsigned flags, uint64_t hugepage_sz)
{
	unsigned check_flag = 0;

	if (!(flags & ~LL_MEMZONE_SIZE_HINT_ONLY))
		return 1;

	switch (hugepage_sz) {
	case LL_PGSIZE_256K:
		check_flag = LL_MEMZONE_256KB;
		break;
	case LL_PGSIZE_2M:
		check_flag = LL_MEMZONE_2MB;
		break;
	case LL_PGSIZE_16M:
		check_flag = LL_MEMZONE_16MB;
		break;
	case LL_PGSIZE_256M:
		check_flag = LL_MEMZONE_256MB;
		break;
	case LL_PGSIZE_512M:
		check_flag = LL_MEMZONE_512MB;
		break;
	case LL_PGSIZE_1G:
		check_flag = LL_MEMZONE_1GB;
		break;
	case LL_PGSIZE_4G:
		check_flag = LL_MEMZONE_4GB;
		break;
	case LL_PGSIZE_16G:
		check_flag = LL_MEMZONE_16GB;
	}

	return check_flag & flags;
}


int malloc_socket_to_heap_id(unsigned int socked_id) {
    struct ll_mem_config *mcfg = ll_get_mem_config();
    int i;
    
    for (i = 0; i < LL_MAX_HEAPS; i++) {
        struct malloc_heap *heap = &mcfg->malloc_heaps[i];
        
        if (heap->socket_id == socked_id) {
            return i;
        }
    }
    return -1;
}

static struct malloc_elem *
malloc_heap_add_memory(struct malloc_heap *heap, struct ll_memseg_list *msl,
    void *start, size_t len, bool dirty) {

    struct malloc_elem *elem = start;

    malloc_elem_init(elem, heap, msl, len, elem, len, dirty);
    
    malloc_elem_insert(elem);
    
    elem = malloc_elem_join_adjacent_free(elem);
    
    malloc_elem_free_list_insert(elem);
    
    return elem;
}

static int
malloc_add_seg(const struct ll_memseg_list *msl, const struct ll_memseg *ms, size_t len, void *arg __ll_unused) {
    struct ll_mem_config *mcfg = ll_get_mem_config();
    struct ll_memseg_list *found_msl;
    struct malloc_heap *heap;
    int msl_idx, heap_idx;
    
    if (msl->external) {
        return 0;
    }

    heap_idx = malloc_socket_to_heap_id(msl->socket_id);
    if (heap_idx < 0) {
        LL_LOG(ERR, EAL, "Memseg list has invalid socket id\n");
        return -1;
    }
    heap = &mcfg->malloc_heaps[heap_idx];
    
    //msl is const ,so find it
    msl_idx = msl - mcfg->memsegs;
    
    if (msl_idx < 0 || msl_idx >= LL_MAX_MEMSEG_LISTS) {
        return -1;
    }

    found_msl = &mcfg->memsegs[msl_idx];
    
    malloc_heap_add_memory(heap, found_msl, ms->addr, len, ms->flags & LL_MEMSEG_FLAG_DIRTY);
    
    heap->total_size += len;
    
    LL_LOG(DEBUG, EAL, "Added %zuM to heap on socket %i\n", len >> 20,
			msl->socket_id);
    return 0;
}

int
ll_eal_malloc_heap_init(void) {
    struct ll_mem_config *mcfg = ll_get_mem_config();
    const struct internal_config *internal_config = ll_eal_get_internal_configuration();
    int i;

    if (internal_config->match_allocations) {
        LL_LOG(DEBUG, EAL, "Hugepages will be freed exactly as allocated.\n");
    }
    
    if (ll_eal_process_type() == LL_PROC_PRIMARY) {

        // assign min socket ID to external heaps
        mcfg->next_socket_id = EXTERNAL_HEAP_MIN_SOCKET_ID;

        //assign names to default heaps
        
        for (i = 0; i< ll_socket_count(); i++) {
            struct malloc_heap *heap = &mcfg->malloc_heaps[i];
            char heap_name[LL_HEAP_NAME_MAX_LEN];
            int socket_id = ll_socket_id_by_idx(i);

            snprintf(heap->name, LL_HEAP_NAME_MAX_LEN, "socket_%i", socket_id);
			heap->socket_id = socket_id;
        }
    } else {
        return 0;
    }
    
    return ll_memseg_contig_walk(malloc_add_seg, NULL);
    
}