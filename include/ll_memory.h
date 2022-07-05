#ifndef _LL_MEMORY_H_
#define _LL_MEMORY_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <ll_common.h>
#include <ll_bitops.h>
#include <ll_fbarray.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LL_PGSIZE_4K   (1ULL << 12)
#define LL_PGSIZE_64K  (1ULL << 16)
#define LL_PGSIZE_256K (1ULL << 18)
#define LL_PGSIZE_2M   (1ULL << 21)
#define LL_PGSIZE_16M  (1ULL << 24)
#define LL_PGSIZE_256M (1ULL << 28)
#define LL_PGSIZE_512M (1ULL << 29)
#define LL_PGSIZE_1G   (1ULL << 30)
#define LL_PGSIZE_4G   (1ULL << 32)
#define LL_PGSIZE_16G  (1ULL << 34)

#define SOCKET_ID_ANY -1                    /**< Any NUMA socket. */
/** Prevent this segment from being freed back to the OS. */
#define LL_MEMSEG_FLAG_DO_NOT_FREE LL_BIT32(0)
/** This segment is not filled with zeros. */
#define LL_MEMSEG_FLAG_DIRTY LL_BIT32(1)

/**
 * Physical memory segment descriptor.
 */
struct ll_memseg {
	ll_iova_t iova;            /**< Start IO address. */
	LL_STD_C11
	union {
		void *addr;         /**< Start virtual address. */
		uint64_t addr_64;   /**< Makes sure addr is always 64 bits */
	};
	size_t len;               /**< Length of the segment. */
	uint64_t hugepage_sz;       /**< The pagesize of underlying memory */
	int32_t socket_id;          /**< NUMA socket ID. */
	uint32_t nchannel;          /**< Number of channels. */
	uint32_t nrank;             /**< Number of ranks. */
	uint32_t flags;             /**< Memseg-specific flags */
} __ll_packed;

#define MEMSEG_LIST_FMT "memseg-%" PRIu64 "k-%i-%i"
/**
 * memseg list is a special case as we need to store a bunch of other data
 * together with the array itself.
 */
struct ll_memseg_list {
	LL_STD_C11
	union {
		void *base_va;
		/**< Base virtual address for this memseg list. */
		uint64_t addr_64;
		/**< Makes sure addr is always 64-bits */
	};
	uint64_t page_sz; /**< Page size for all memsegs in this list. */
	int socket_id; /**< Socket ID for all memsegs in this list. */
	volatile uint32_t version; /**< version number for multiprocess sync. */
	size_t len; /**< Length of memory area covered by this memseg list. */
	unsigned int external; /**< 1 if this list points to external memory */
	unsigned int heap; /**< 1 if this list points to a heap */
	struct ll_fbarray memseg_arr;
};

/**
 * Memseg walk function prototype.
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*ll_memseg_walk_t)(const struct ll_memseg_list *msl,
		const struct ll_memseg *ms, void *arg);

/**
 * Memseg contig walk function prototype. This will trigger a callback on every
 * VA-contiguous area starting at memseg ``ms``, so total valid VA space at each
 * callback call will be [``ms->addr``, ``ms->addr + len``).
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*ll_memseg_contig_walk_t)(const struct ll_memseg_list *msl,
		const struct ll_memseg *ms, size_t len, void *arg);

/**
 * Memseg list walk function prototype. This will trigger a callback on every
 * allocated memseg list.
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*ll_memseg_list_walk_t)(const struct ll_memseg_list *msl,
		void *arg);

/**
 * Walk list of all memsegs.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @note This function will also walk through externally allocated segments. It
 *       is up to the user to decide whether to skip through these segments.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function repolld error
 */
int
ll_memseg_walk(ll_memseg_walk_t func, void *arg);

/**
 * Walk each VA-contiguous area.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @note This function will also walk through externally allocated segments. It
 *       is up to the user to decide whether to skip through these segments.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function repolld error
 */
int
ll_memseg_contig_walk(ll_memseg_contig_walk_t func, void *arg);

/**
 * Walk each allocated memseg list.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @note This function will also walk through externally allocated segments. It
 *       is up to the user to decide whether to skip through these segments.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function repolld error
 */
int
ll_memseg_list_walk(ll_memseg_list_walk_t func, void *arg);






static inline void *
ll_zmalloc(const char *name, size_t size, int pad) {
	void *ret = NULL;
	ret = malloc(size);
	if (ret == NULL) {
		return NULL;
	}
	memset(ret, pad, size);
	
	return ret;
}


static inline void *
ll_zmalloc_socket(const char *name, size_t size, int pad, int socket_id) {
	return ll_zmalloc(name, size, pad);
}

static inline void
ll_free(void * mem) {
	free(mem);
}

static inline void *
ll_memcpy(void * dst, const void *src, size_t size) {
	return memcpy(dst, src, size);
}

static inline void *
ll_memset(void *s, int c, size_t n) {
	return memset(s, c, n);
}

static inline struct ll_memseg *
ll_mem_virt2memseg(const void *addr, const struct ll_memseg_list *msl)
{
	return NULL;
}


ll_phys_addr_t ll_mem_vir2phy(const void *virt);

ll_iova_t ll_mem_virt2iova(const void *virt);

//get sys mem_page_size
size_t ll_mem_page_size(void);


#ifdef __cplusplus
}
#endif

#endif /* _LL_MEMORY_H_ */
