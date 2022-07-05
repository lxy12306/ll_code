#ifndef _LL_CONFIG_H_
#define _LL_CONFIG_H_

#include <stdint.h>

#include <ll_eal.h>
#include <ll_thread_lcore.h>

#include "../common/common_config.h"


LL_DECLARE_PER_LCORE(unsigned, _lcore_id);

extern struct lcore_config lcore_config[LL_MAX_LCORE];

struct ll_mem_config;

struct ll_config {

	uint32_t main_lcore;         /**< Id of the main lcore */
	uint32_t lcore_count;        /**< Number of available logical cores. */
	uint32_t numa_node_count;    /**< Number of detected NUMA nodes. */
	uint32_t numa_nodes[LL_MAX_NUMA_NODES]; /**< List of detected NUMA nodes. */
   
    enum ll_iova_mode iova_mode; // PA or VA mapping mode
    enum ll_proc_type_t process_type;
    enum ll_lcore_role_t lcore_role[LL_MAX_LCORE]; /**< State of cores. */

	// Pointer to memory configuration, which may be shared across multiple instances
    struct ll_mem_config *mem_config;

};

struct internal_config {
	volatile size_t memory;           /**< amount of asked memory */
	volatile unsigned no_hugetlbfs;   /**< true to disable hugetlbfs */

	uintptr_t base_virtaddr;          /**< base address to try and reserve memory from */
    volatile unsigned legacy_mem;
	/**< true to enable legacy memory behavior (no dynamic allocation,
	 * IOVA-contiguous segments).
	 */
    volatile unsigned match_allocations;
	/**< true to free hugepages exactly as allocated */
	volatile unsigned single_file_segments;
	/**< true if storing all pages within single files (per-page-size,
	 * per-node) non-legacy mode only.
	 */

};


/* Return a pointer to the configuration structure */
struct ll_config *
ll_eal_get_configuration(void);


/* Return a pointer to the internal configuration structure */
struct internal_config *
ll_eal_get_internal_configuration(void);


/**
 * Return the Application thread ID of the execution unit.
 *
 * Note: in most cases the lcore id returned here will also correspond
 *   to the processor id of the CPU on which the thread is pinned, this
 *   will not be the case if the user has explicitly changed the thread to
 *   core affinities using --lcores EAL argument e.g. --lcores '(0-3)@10'
 *   to run threads with lcore IDs 0, 1, 2 and 3 on physical core 10..
 *
 * @return
 *  Logical core ID (in EAL thread or registered non-EAL thread) or
 *  LL_LCORE_ID_ANY (in unregistered non-EAL thread)
 */
static inline unsigned
ll_lcore_id(void) {
    return LL_PER_LCORE(_lcore_id);
}

/**
 * @return
 *  Return the can used max socket count
 */
unsigned int
ll_socket_count(void);

/**
 * @param
 *  the socket index
 * @return
 *  Return the socket id
 */
int
ll_socket_id_by_idx(unsigned int idx);


//check if there has hugepages
int
ll_eal_has_hugepages(void);

//check iova mode
enum ll_iova_mode
ll_eal_iova_mode(void);


/*** arch defines ***/
#define LL_ARCH_64 1
#define LL_ARCH_X86_64 1


#endif	//_LL_CONFIG_H_
