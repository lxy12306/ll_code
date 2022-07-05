#ifndef _LL_MEM_CONFIG_H_
#define _LL_MEM_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ll_rwlock.h>
#include <ll_memory.h>

#include "../mem/malloc_heap.h"

/**
 * Memory configuration shared across multiple processes.
 */
struct ll_mem_config {
	ll_rwlock_t mplock; //used by mempool library for thread safety

	ll_rwlock_t memory_hotplug_lock;
	//Indicates whether memory hotplug request is in progress.

	struct ll_memseg_list memsegs[LL_MAX_MEMSEG_LISTS];
	
	struct malloc_heap malloc_heaps[LL_MAX_HEAPS];
	uint32_t next_socket_id; //next socket ID for external malloc heap
};


/**
 * Get the global ll_mem_config.
 *
 * @return
 * 	the global ll_mem_config
 */
struct ll_mem_config *
ll_get_mem_config(void);


/*** mem config check ***/

/**
 * Get the iova mode
 *
 * @return
 *   enum ll_iova_mode value.
 */
enum ll_iova_mode ll_mcfg_iova_mode(void);


/*** mem read-write lock ***/
void
ll_mcfg_mem_read_lock(void);

void
ll_mcfg_mem_read_unlock(void);

void
ll_mcfg_mem_write_lock(void);

void
ll_mcfg_mem_write_unlock(void);

/*** mempool read-write lock ***/
void ll_mcfg_mempool_read_lock(void);

void ll_mcfg_mempool_read_unlock(void);

void ll_mcfg_mempool_write_lock(void);

void ll_mcfg_mempool_write_unlock(void);


#ifdef __cplusplus
}
#endif
#endif //_LL_MEM_CONFIG_H_
