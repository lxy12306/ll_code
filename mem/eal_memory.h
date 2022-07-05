#ifndef _EAL_MEMOEY_H
#define _EAL_MEMOEY_H

#include <ll_memory.h>

#define EAL_VIRTUAL_AREA_ADDR_IS_HINT (1 << 0)
/**< don't fail if cannot get exact requested address. */
#define EAL_VIRTUAL_AREA_ALLOW_SHRINK (1 << 1)
/**< try getting smaller sized (decrement by page size) virtual areas if cannot
 * get area of requested size.
 */
#define EAL_VIRTUAL_AREA_UNMAP (1 << 2)
/**< immediately unmap reserved virtual area. */


/**
 * Memory reservation flags.
 */
enum eal_mem_reserve_flags {
	/**
	 * Reserve hugepages. May be unsupported by some platforms.
	 */
	EAL_RESERVE_HUGEPAGES = 1 << 0,
	/**
	 * Force reserving memory at the requested address.
	 * This can be a destructive action depending on the implementation.
	 *
	 * @see RTE_MAP_FORCE_ADDRESS for description of possible consequences
	 *      (although implementations are not required to use it).
	 */
	EAL_RESERVE_FORCE_ADDRESS = 1 << 1
};


void *
eal_mem_reserve(void *requested_addr, size_t size, int flags);

int
eal_mem_unmap(void *virt, size_t size);

/**< immediately unmap reserved virtual area*/
void *
eal_get_virtual_area(void *requested_addr, size_t *size,
	size_t page_sz, int flags, int reserve_flags);


int
ll_memseg_walk(ll_memseg_walk_t func, void *arg);

int
ll_memseg_contig_walk(ll_memseg_contig_walk_t func, void *arg);

int
ll_memseg_list_walk(ll_memseg_list_walk_t func, void *arg);


//init memory subsystem
int
ll_eal_memory_init(void);

#endif