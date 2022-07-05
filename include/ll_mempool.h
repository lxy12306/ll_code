#ifndef _LL_MEMPOOL_H_
#define _LL_MEMPOOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#include <ll_config.h>
#include <ll_common.h>
#include <ll_debug.h>
#include <ll_list.h>
#include <ll_spinlock.h>

#include <ll_memzone.h>
#include <ll_memory.h>
#include <ll_ring.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LL_MEMPOOL_MZ_PREFIX "MP_"
#define LL_MEMPOOL_NAMESIZE (LL_RING_NAMESIZE -	\
			sizeof(LL_MEMPOOL_MZ_PREFIX) + 1)
/* "MP_<name>" */
#define	LL_MEMPOOL_MZ_FORMAT	LL_MEMPOOL_MZ_PREFIX "%s"

#ifndef LL_MEMPOOL_ALIGN
#define LL_MEMPOOL_ALIGN LL_CACHE_LINE_SIZE
#endif

#define LL_MEMPOOL_ALIGN_MASK (LL_MEMPOOL_ALIGN - 1)




#ifdef LL_LIBLL_MEMPOOL_DEBUG
	
struct ll_mempool_debug_stats {
	uint64_t put_bulk;             /**< Number of puts. */
	uint64_t put_objs;             /**< Number of objects successfully put. */
	uint64_t put_common_pool_bulk; /**< Number of bulks enqueued in common pool. */
	uint64_t put_common_pool_objs; /**< Number of objects enqueued in common pool. */
	uint64_t get_common_pool_bulk; /**< Number of bulks dequeued from common pool. */
	uint64_t get_common_pool_objs; /**< Number of objects dequeued from common pool. */
	uint64_t get_success_bulk;     /**< Successful allocation number. */
	uint64_t get_success_objs;     /**< Objects successfully allocated. */
	uint64_t get_fail_bulk;        /**< Failed allocation number. */
	uint64_t get_fail_objs;        /**< Objects that failed to be allocated. */
	uint64_t get_success_blks;     /**< Successful allocation number of contiguous blocks. */
	uint64_t get_fail_blks;        /**< Failed allocation number of contiguous blocks. */
} __ll_cache_aligned;

#endif

/**
 * @internal When debug is enabled, store some statistics.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @param name
 *   Name of the statistics field to increment in the memory pool.
 * @param n
 *   Number to add to the object-oriented statistics.
 */
#ifdef LL_LIBLL_MEMPOOL_DEBUG
#define LL_MEMPOOL_STAT_ADD(mp, name, n) do {                  \
		mp->stats.name += n;        \
	} while (0)
#else
#define LL_MEMPOOL_STAT_ADD(mp, name, n) do {} while (0)
#endif

/**
 * A structure that stores a per_core object cache.
 */
struct ll_mempool_cache {
	uint32_t size; //Size of the cache
	uint32_t flushthresh; //Threshold before we flush excess elements
	uint32_t len; //current cache count

	/*
	 * Cache is allocated to this size to allow it to overflow in certain
	 * cases to avoid needless emptying of cache
	 */
	
	void *objs[LL_MEMPOOL_CACHE_MAX_SIZE * 2];

} __ll_cache_aligned;

/**
 * A structure that stores the size of mempool elements
 */
struct ll_mempool_objsz {
	uint32_t elt_size; //size of an element
	uint32_t header_size; //size of header
	uint32_t trailer_size; //size of trailer
	uint32_t total_size;
};


struct ll_mempool;

/**
 * Mempool object header structure
 *
 * Each object stored in mempools are prefixed by this header structure
 */
struct ll_mempool_objhdr {
	LL_STAILQ_ENTRY(ll_mempool_objhdr) next; //next in list
	struct ll_mempool *mp;	//the mempool owning the object
	LL_STD_C11
	union {
		ll_iova_t iova; //IO address of the object
		ll_phys_addr_t  phys_addr; //Physical address of the object
	};
#ifdef LL_LIBLL_MEMPOOL_DEBUG
	uint64_t cookie; //debug cookie
#endif
};

#ifdef LL_LIBLL_MEMPOOL_DEBUG
#define LL_MEMPOOL_CHECK_COOKIES(mp, obj_table_const, n, free) \
	ll_mempool_check_cookies(mp, obj_table_const, n, free)
#else
#define LL_MEMPOOL_CHECK_COOKIES(mp, obj_table_const, n, free) do {} while (0)
#endif /* LL_LIBLL_MEMPOOL_DEBUG */


/**
 * A list of object headers type
 */
LL_STAILQ_HEAD(ll_mempool_objhdr_list,ll_mempool_objhdr);

/**	
 * Returun the header of a mempool object (internal)
 * 
 * @param obj
 *	An object that is owned by a pool. If this is not the case,
 *	the behavior is undefined.
 *
 * @return
 *	A pointer to the object header.
 */
static inline struct ll_mempool_objhdr *
ll_mempool_get_header(void *obj) {
	return (struct ll_mempool_objhdr *)LL_PTR_SUB(obj,
				sizeof(struct ll_mempool_objhdr));
}

#ifdef LL_LIBLL_MEMPOOL_DEBUG
/**
 * In debug mode, each object stored in mempools are suffixed by this
 * trailer structure containing a cookie preventing memory corruptions
 */

struct ll_mempool_objtlr {
	uint64_t cookie;	//debug cookie
};
#endif


struct ll_mempool_memhdr;
/**
 * Callback used to free a memory chunk
 */
typedef void (ll_mempool_memchunk_free_cb_t)(struct ll_mempool_memhdr *memhdr,
	void *opaque);


/**
 * The memory chunks where objects are stored.
 */
struct ll_mempool_memhdr {
	LL_STAILQ_ENTRY(ll_mempool_memhdr) next; //next in list
	struct ll_mempool *mp;	//the mempool owning the chunks
	void *addr;	//virtual address of the chunk
	LL_STD_C11	
	union {
		ll_iova_t iova; //IO address of the object
		ll_phys_addr_t  phys_addr; //Physical address of the object
	};
	size_t len;
	ll_mempool_memchunk_free_cb_t *free_cb; //free callback
	void *opaque;
};

/**
 * The list of memory where objects are stored
 */
LL_STAILQ_HEAD(ll_mempool_memhdr_list, ll_mempool_memhdr);


struct ll_mempool {
	char name[LL_MEMPOOL_NAMESIZE];
	LL_STD_C11
	union {
		void* pool_data; //Ring or pool to store object
		uint64_t pool_id; //External mempool identifier
	};

	void *pool_config;               /**< optional args for ops alloc. */
	const struct ll_memzone *mz;    /**< Memzone where pool is alloc'd. */
	unsigned int flags;              /**< Flags of the mempool. */
	int socket_id;                   /**< Socket id passed at create. */

	uint32_t size; //max_size of the mempool
	uint32_t cache_size; //size of per-lcore default local cache
	uint32_t private_data_size; //size of private data

	uint32_t elt_size; //size of an element
	uint32_t header_size; //size of header(before elt)
	uint32_t trailer_size; //size of trailer (after elt)

	int32_t ops_index;//index to the ops
	struct ll_mempool_cache *local_cache;//per-lcore local cache

	uint32_t populated_size; //number of populated objects
	struct ll_mempool_objhdr_list elt_list; //list of objects in pool

	uint32_t nb_mem_chunks; //number of memory chunks
	struct ll_mempool_memhdr_list mem_list; //list of memory chunks

#ifdef LL_LIBLL_MEMPOOL_DEBUG
	struct ll_mempool_debug_stats stats;
#endif

}__ll_cache_aligned;

/**
 * A mempool constructor callback function.
 *
 * Arguments are the mempool and the opaque pointer given by the user in
 * ll_mempool_create().
 */
typedef void (ll_mempool_ctor_t)(struct ll_mempool *, void *);


/**
 * An object callback function for mempool.
 *
 * Used by ll_mempool_create() and ll_mempool_obj_iter().
 */
typedef void (ll_mempool_obj_cb_t)(struct ll_mempool *mp,
		void *opaque, void *obj, unsigned obj_idx);
typedef ll_mempool_obj_cb_t ll_mempool_obj_ctor_t; /* compat */

#define LL_MEMPOOL_F_NO_SPREAD 0x0001 //spreading among memory channels not required 
#define LL_MEMPOOL_F_NO_CACHE_ALIGN 0x0002
#define LL_MEMPOOL_F_SP_PUT 0x0004
#define LL_MEMPOOL_F_SC_GET 0x0008


#define LL_MEMPOOL_F_POOL_CREATED	0x0010 //Internal: pool is created.

#define LL_MEMPOOL_F_NO_IOVA_CONTIG	0x0020 //Don't need IOVA contiguous objects.

#define LL_MEMPOOL_F_NON_IO 0x0040 //Internal: no object from the pool can be used for device IO(DMA)


#define LL_MEMPOOL_VALID_USER_FLAGS (LL_MEMPOOL_F_NO_SPREAD \
	| LL_MEMPOOL_F_NO_CACHE_ALIGN	\
	| LL_MEMPOOL_F_SP_PUT	\
	| LL_MEMPOOL_F_SC_GET	\
	| LL_MEMPOOL_F_NO_IOVA_CONTIG \
)

#define LL_MEMPOOL_HEADER_SIZE(mp, cs)	\
	(sizeof(*(mp)) + (((cs) == 0) ? 0 : sizeof(struct ll_mempool_cache) * LL_MAX_LCORE))


/**	
 * Returun the page size of mem of mempool
 * 
 * @param[in] mp
 *	The mempool
 * 
 * @param[out] pg_sz
 * 
 *	The location of return of page size
 * 
 * @return
 *	success or not
 */
int
ll_mempool_get_page_size(struct ll_mempool *mp, size_t *pg_sz);


/**
 * Mempool event type.
 * @internal
 */
enum ll_mempool_event {
	/** Occurs after a mempool is fully populated. */
	LL_MEMPOOL_EVENT_READY = 0,
	/** Occurs before the destruction of a mempool begins. */
	LL_MEMPOOL_EVENT_DESTROY = 1,
};



/*** mempool ops ***/
typedef int (*ll_mempool_alloc_t)(struct ll_mempool *mp);

typedef void (*ll_mempool_free_t)(struct ll_mempool *mp);

typedef int (*ll_mempool_enqueue_t)(struct ll_mempool *mp, void * const *obj_table, unsigned int n);

typedef int (*ll_mempool_dequeue_t)(struct ll_mempool *mp, void **obj_table, unsigned int n);

typedef int (*ll_mempool_dequeue_contig_blocks_t)(struct ll_mempool *mp, void **first_obj_table, unsigned int n);

typedef unsigned (*ll_mempool_get_count_t)(const struct ll_mempool *mp);

/**
 * Calculate memory size required to store given number of objects.
 *
 * If mempool objects are not required to be IOVA-contiguous
 * (the flag LL_MEMPOOL_F_NO_IOVA_CONTIG is set),min_chunk_size defines virtually contiguous chunk size.
 * Otherwise, if mempool objects must be IOVA-contiguous (the flag LL_MEMPOOL_F_NO_IOVA_CONTIG is clear),
 * min_chunk_size defines  IOVA-contiguous chunk size
 *
 * @param[in] mp
 *	Pointer to the memory pool.
 * @param[in] obj_num
 *  Number of objects.
 * @param[in] pg_shift
 *  LOG2 of the physical pages size. If set to 0, ignore page boundaries.
 * @param[out] min_chunk_size
 *  Location for minimum size of the memory chunk which may be used to store memory pool objects
 * @param[out] align
 *  Location for required memory chunk alignment
 * @return
 *  Required memory size
 */
typedef ssize_t (*ll_mempool_calc_mem_size_t)(const struct ll_mempool *mp, uint32_t obj_num, uint32_t pg_shift,
			size_t *min_chunk_size, size_t *align);

/**
 * Align objects on addresses multiple of total_elt_sz.
 */
#define LL_MEMPOOL_POPULATE_F_ALIGN_OBJ 0x0001

/**
 * Function to be called for each populated object.
 *
 * @param[in] mp
 *   A pointer to the mempool structure.
 * @param[in] opaque
 *   An opaque pointer passed to iterator.
 * @param[in] vaddr
 *   Object virtual address.
 * @param[in] iova
 *   Input/output virtual address of the object or LL_BAD_IOVA.
 */
typedef void (ll_mempool_populate_obj_cb_t)(struct ll_mempool *mp, void *opaque, void *vaddr, ll_iova_t iova);

/**
 * @internal wrapper for mempool_ops populate callback.
 *
 * Populate memory pool objects using provided memory chunk.
 *
 * @param[in] mp
 *   A pointer to the mempool structure.
 * @param[in] max_objs
 *   Maximum number of objects to be populated.
 * @param[in] vaddr
 *   The virtual address of memory that should be used to store objects.
 * @param[in] iova
 *   The IO address
 * @param[in] len
 *   The length of memory in bytes.
 * @param[in] obj_cb
 *   Callback function to be executed for each populated object.
 * @param[in] obj_cb_arg
 *   An opaque pointer passed to the callback function.
 * @return
 *   The number of objects added on success.
 *   On error, no objects are populated and a negative errno is returned.
 */
typedef int (*ll_mempool_populate_t)(struct ll_mempool *mp,
		unsigned int max_objs,
		void *vaddr, ll_iova_t iova, size_t len,
		ll_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg);

struct ll_mempool_info {
	//Number of objects in the contiguous block
	unsigned int contig_block_size;
} __ll_cache_aligned;

typedef int (*ll_mempool_get_info_t)(const struct ll_mempool *mp, struct ll_mempool_info *info);

#define LL_MEMPOOL_OPS_NAMESIZE 32

//structure defining mempool operations structure
struct ll_mempool_ops {
	char name[LL_MEMPOOL_OPS_NAMESIZE];
	ll_mempool_alloc_t alloc;
	ll_mempool_free_t free;
	ll_mempool_enqueue_t enqueue;
	ll_mempool_dequeue_t dequeue;
	ll_mempool_get_count_t get_count;
	
	ll_mempool_calc_mem_size_t calc_mem_size;

	ll_mempool_populate_t populate;
	
	ll_mempool_get_info_t get_info;
	
	ll_mempool_dequeue_contig_blocks_t dequeue_contig_blocks;
} __ll_cache_aligned;

#define LL_MEMPOOL_MAX_OPS_IDX 16  /**< Max registered ops structs */

/**
 * Structure storing the table of registered ops structs, each of which contain
 * the function pointers for the mempool ops functions.
 * Each process has its own storage for this ops struct array so that
 * the mempools can be shared across primary and secondary processes.
 * The indices used to access the array are valid across processes, whereas
 * any function pointers stored directly in the mempool struct would not be.
 * This results in us simply having "ops_index" in the mempool struct.
 */
struct ll_mempool_ops_table {
	ll_spinlock_t sl;     /**< Spinlock for add/delete. */
	uint32_t num_ops;      /**< Number of used ops structs in the table. */
	/**
	 * Storage for all possible ops structs.
	 */
	struct ll_mempool_ops ops[LL_MEMPOOL_MAX_OPS_IDX];
} __ll_cache_aligned;

extern struct ll_mempool_ops_table ll_mempool_ops_table;

/**
 * @internal Get the mempool ops struct from its index.
 *
 * @param ops_index
 *   The index of the ops struct in the ops struct table. It must be a valid
 *   index: (0 <= idx < num_ops).
 * @return
 *   The pointer to the ops struct in the table.
 */
static inline struct ll_mempool_ops *
ll_mempool_get_ops(int ops_index)
{
	LL_VERIFY((ops_index >= 0) && (ops_index < LL_MEMPOOL_MAX_OPS_IDX));

	return &ll_mempool_ops_table.ops[ops_index];
}

/**
 * @internal Wrapper for mempool_ops alloc callback.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @return
 *   - 0: Success; successfully allocated mempool pool_data.
 *   - <0: Error; code of alloc function.
 */
int
ll_mempool_ops_alloc(struct ll_mempool *mp);

/**
 * @internal Wrapper for mempool_ops dequeue callback.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @param obj_table
 *   Pointer to a table of void * pointers (objects).
 * @param n
 *   Number of objects to get.
 * @return
 *   - 0: Success; got n objects.
 *   - <0: Error; code of dequeue function.
 */
static inline int
ll_mempool_ops_dequeue_bulk(struct ll_mempool *mp,
		void **obj_table, unsigned n)
{
	struct ll_mempool_ops *ops;
	int ret;
	ops = ll_mempool_get_ops(mp->ops_index);
	ret = ops->dequeue(mp, obj_table, n);
	if (ret == 0) {
		LL_MEMPOOL_STAT_ADD(mp, get_common_pool_bulk, 1);
		LL_MEMPOOL_STAT_ADD(mp, get_common_pool_objs, n);
	}
	return ret;
}


/**
 * @internal Wrapper for mempool_ops dequeue_contig_blocks callback.
 *
 * @param[in] mp
 *   Pointer to the memory pool.
 * @param[out] first_obj_table
 *   Pointer to a table of void * pointers (first objects).
 * @param[in] n
 *   Number of blocks to get.
 * @return
 *   - 0: Success; got n objects.
 *   - <0: Error; code of dequeue function.
 */
static inline int
ll_mempool_ops_dequeue_contig_blocks(struct ll_mempool *mp,
		void **first_obj_table, unsigned int n)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);
	LL_ASSERT(ops->dequeue_contig_blocks != NULL);
	return ops->dequeue_contig_blocks(mp, first_obj_table, n);
}
/**
 * @internal wrapper for mempool_ops enqueue callback.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @param obj_table
 *   Pointer to a table of void * pointers (objects).
 * @param n
 *   Number of objects to put.
 * @return
 *   - 0: Success; n objects supplied.
 *   - <0: Error; code of enqueue function.
 */
static inline int
ll_mempool_ops_enqueue_bulk(struct ll_mempool *mp, void * const *obj_table,
		unsigned n)
{
	struct ll_mempool_ops *ops;

	LL_MEMPOOL_STAT_ADD(mp, put_common_pool_bulk, 1);
	LL_MEMPOOL_STAT_ADD(mp, put_common_pool_objs, n);
	ops = ll_mempool_get_ops(mp->ops_index);
	return ops->enqueue(mp, obj_table, n);
}

/**
 * @internal wrapper for mempool_ops get_count callback.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @return
 *   The number of available objects in the external pool.
 */
unsigned
ll_mempool_ops_get_count(const struct ll_mempool *mp);

/**
 * @internal wrapper for mempool_ops calc_mem_size callback.
 * API to calculate size of memory required to store specified number of
 * object.
 *
 * @param[in] mp
 *   Pointer to the memory pool.
 * @param[in] obj_num
 *   Number of objects.
 * @param[in] pg_shift
 *   LOG2 of the physical pages size. If set to 0, ignore page boundaries.
 * @param[out] min_chunk_size
 *   Location for minimum size of the memory chunk which may be used to
 *   store memory pool objects.
 * @param[out] align
 *   Location for required memory chunk alignment.
 * @return
 *   Required memory size aligned at page boundary.
 */
ssize_t ll_mempool_ops_calc_mem_size(const struct ll_mempool *mp,
				      uint32_t obj_num, uint32_t pg_shift,
				      size_t *min_chunk_size, size_t *align);

/**
 * @internal wrapper for mempool_ops populate callback.
 *
 * Populate memory pool objects using provided memory chunk.
 *
 * @param[in] mp
 *   A pointer to the mempool structure.
 * @param[in] max_objs
 *   Maximum number of objects to be populated.
 * @param[in] vaddr
 *   The virtual address of memory that should be used to store objects.
 * @param[in] iova
 *   The IO address
 * @param[in] len
 *   The length of memory in bytes.
 * @param[in] obj_cb
 *   Callback function to be executed for each populated object.
 * @param[in] obj_cb_arg
 *   An opaque pointer passed to the callback function.
 * @return
 *   The number of objects added on success.
 *   On error, no objects are populated and a negative errno is returned.
 */
int ll_mempool_ops_populate(struct ll_mempool *mp, unsigned int max_objs,
			     void *vaddr, ll_iova_t iova, size_t len,
			     ll_mempool_populate_obj_cb_t *obj_cb,
			     void *obj_cb_arg);

/**
 * Wrapper for mempool_ops get_info callback.
 *
 * @param[in] mp
 *   Pointer to the memory pool.
 * @param[out] info
 *   Pointer to the ll_mempool_info structure
 * @return
 *   - 0: Success; The mempool driver supports retrieving supplementary
 *        mempool information
 *   - -ENOTSUP - doesn't support get_info ops (valid case).
 */
int ll_mempool_ops_get_info(const struct ll_mempool *mp,
			 struct ll_mempool_info *info);

/**
 * @internal wrapper for mempool_ops free callback.
 *
 * @param mp
 *   Pointer to the memory pool.
 */
void
ll_mempool_ops_free(struct ll_mempool *mp);

/**
 * Set the ops of a mempool.
 *
 * This can only be done on a mempool that is not populated, i.e. just after
 * a call to ll_mempool_create_empty().
 *
 * @param mp
 *   Pointer to the memory pool.
 * @param name
 *   Name of the ops structure to use for this mempool.
 * @param pool_config
 *   Opaque data that can be passed by the application to the ops functions.
 * @return
 *   - 0: Success; the mempool is now using the requested ops functions.
 *   - -EINVAL - Invalid ops struct name provided.
 *   - -EEXIST - mempool already has an ops struct assigned.
 */
int
ll_mempool_set_ops_byname(struct ll_mempool *mp, const char *name,
		void *pool_config);

/**
 * Register mempool operations.
 *
 * @param ops
 *   Pointer to an ops structure to register.
 * @return
 *   - >=0: Success; return the index of the ops struct in the table.
 *   - -EINVAL - some missing callbacks while registering ops struct.
 *   - -ENOSPC - the maximum number of ops structs has been reached.
 */
int ll_mempool_register_ops(const struct ll_mempool_ops *ops);

/**
 * Macro to statically register the ops of a mempool handler.
 * Note that the ll_mempool_register_ops fails silently here when
 * more than LL_MEMPOOL_MAX_OPS_IDX is registered.
 */
#define LL_MEMPOOL_REGISTER_OPS(ops)	\
	LL_INIT(mp_hdlr_init_##ops)	\
	{	\
		ll_mempool_register_ops(&ops);	\
	}

#define MEMPOOL_REGISTER_OPS(ops) \
	LL_MEMPOOL_REGISTER_OPS(ops)



/**
 * @internal Helper to calculate memory size required to store given
 * number of objects.
 *
 * This function is internal to mempool library and mempool drivers.
 *
 * If page boundaries may be ignored, it is just a product of total
 * object size including header and trailer and number of objects.
 * Otherwise, it is a number of pages required to store given number of
 * objects without crossing page boundary.
 *
 * Note that if object size is bigger than page size, then it assumes
 * that pages are grouped in subsets of physically continuous pages big
 * enough to store at least one object.
 *
 * Minimum size of memory chunk is the total element size.
 * Required memory chunk alignment is the cache line size.
 *
 * @param[in] mp
 *   A pointer to the mempool structure.
 * @param[in] obj_num
 *   Number of objects to be added in mempool.
 * @param[in] pg_shift
 *   LOG2 of the physical pages size. If set to 0, ignore page boundaries.
 * @param[in] chunk_reserve
 *   Amount of memory that must be reserved at the beginning of each page,
 *   or at the beginning of the memory area if pg_shift is 0.
 * @param[out] min_chunk_size
 *   Location for minimum size of the memory chunk which may be used to
 *   store memory pool objects.
 * @param[out] align
 *   Location for required memory chunk alignment.
 * @return
 *   Required memory size.
 */
ssize_t ll_mempool_op_calc_mem_size_helper(const struct ll_mempool *mp,
		uint32_t obj_num, uint32_t pg_shift, size_t chunk_reserve,
		size_t *min_chunk_size, size_t *align);

/**
 * Default way to calculate memory size required to store given number of
 * objects.
 *
 * Equivalent to ll_mempool_op_calc_mem_size_helper(mp, obj_num, pg_shift,
 * 0, min_chunk_size, align).
 */
ssize_t ll_mempool_op_calc_mem_size_default(const struct ll_mempool *mp,
		uint32_t obj_num, uint32_t pg_shift,
		size_t *min_chunk_size, size_t *align);


/**
 * @internal Helper to populate memory pool object using provided memory
 * chunk: just slice objects one by one, taking care of not
 * crossing page boundaries.
 *
 * If LL_MEMPOOL_POPULATE_F_ALIGN_OBJ is set in flags, the addresses
 * of object headers will be aligned on a multiple of total_elt_sz.
 * This feature is used by octeontx hardware.
 *
 * This function is internal to mempool library and mempool drivers.
 *
 * @param[in] mp
 *   A pointer to the mempool structure.
 * @param[in] flags
 *   Logical OR of following flags:
 *   - LL_MEMPOOL_POPULATE_F_ALIGN_OBJ: align objects on addresses
 *     multiple of total_elt_sz.
 * @param[in] max_objs
 *   Maximum number of objects to be added in mempool.
 * @param[in] vaddr
 *   The virtual address of memory that should be used to store objects.
 * @param[in] iova
 *   The IO address corresponding to vaddr, or LL_BAD_IOVA.
 * @param[in] len
 *   The length of memory in bytes.
 * @param[in] obj_cb
 *   Callback function to be executed for each populated object.
 * @param[in] obj_cb_arg
 *   An opaque pointer passed to the callback function.
 * @return
 *   The number of objects added in mempool.
 */
int ll_mempool_op_populate_helper(struct ll_mempool *mp,
		unsigned int flags, unsigned int max_objs,
		void *vaddr, ll_iova_t iova, size_t len,
		ll_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg);

/**
 * Default way to populate memory pool object using provided memory chunk.
 *
 * Equivalent to ll_mempool_op_populate_helper(mp, 0, max_objs, vaddr, iova,
 * len, obj_cb, obj_cb_arg).
 */
int ll_mempool_op_populate_default(struct ll_mempool *mp,
		unsigned int max_objs,
		void *vaddr, ll_iova_t iova, size_t len,
		ll_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg);


/**
 * Call a function for each mempool element
 *
 * Iterate across all objects attached to a ll_mempool and call the
 * callback function on it.
 *
 * @param mp
 *   A pointer to an initialized mempool.
 * @param obj_cb
 *   A function pointer that is called for each object.
 * @param obj_cb_arg
 *   An opaque pointer passed to the callback function.
 * @return
 *   Number of objects iterated.
 */
uint32_t ll_mempool_obj_iter(struct ll_mempool *mp,
	ll_mempool_obj_cb_t *obj_cb, void *obj_cb_arg);


/**
 * Create a new mempool named *name* in memory.
 *
 * This function uses ``ll_memzone_reserve()`` to allocate memory. The
 * pool contains n elements of elt_size. Its size is set to n.
 *
 * @param name
 *   The name of the mempool.
 * @param n
 *   The number of elements in the mempool. The optimum size (in terms of
 *   memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param elt_size
 *   The size of each element.
 * @param cache_size
 *   If cache_size is non-zero, the ll_mempool library will try to
 *   limit the accesses to the common lockless pool, by maintaining a
 *   per-lcore object cache. This argument must be lower or equal to
 *   LL_MEMPOOL_CACHE_MAX_SIZE and n / 1.5. It is advised to choose
 *   cache_size to have "n modulo cache_size == 0": if this is
 *   not the case, some elements will always stay in the pool and will
 *   never be used. The access to the per-lcore table is of course
 *   faster than the multi-producer/consumer pool. The cache can be
 *   disabled if the cache_size argument is set to 0; it can be useful to
 *   avoid losing objects in cache.
 * @param private_data_size
 *   The size of the private data appended after the mempool
 *   structure. This is useful for storing some private data after the
 *   mempool structure, as is done for ll_mbuf_pool for example.
 * @param mp_init
 *   A function pointer that is called for initialization of the pool,
 *   before object initialization. The user can initialize the private
 *   data in this function if needed. This parameter can be NULL if
 *   not needed.
 * @param mp_init_arg
 *   An opaque pointer to data that can be used in the mempool
 *   constructor function.
 * @param obj_init
 *   A function pointer that is called for each object at
 *   initialization of the pool. The user can set some meta data in
 *   objects if needed. This parameter can be NULL if not needed.
 *   The obj_init() function takes the mempool pointer, the init_arg,
 *   the object pointer and the object number as parameters.
 * @param obj_init_arg
 *   An opaque pointer to data that can be used as an argument for
 *   each call to the object constructor function.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The *flags* arguments is an OR of following flags:
 *   - LL_MEMPOOL_F_NO_SPREAD: By default, objects addresses are spread
 *     between channels in RAM: the pool allocator will add padding
 *     between objects depending on the hardware configuration. See
 *     Memory alignment constraints for details. If this flag is set,
 *     the allocator will just align them to a cache line.
 *   - LL_MEMPOOL_F_NO_CACHE_ALIGN: By default, the returned objects are
 *     cache-aligned. This flag removes this constraint, and no
 *     padding will be present between objects. This flag implies
 *     LL_MEMPOOL_F_NO_SPREAD.
 *   - LL_MEMPOOL_F_SP_PUT: If this flag is set, the default behavior
 *     when using ll_mempool_put() or ll_mempool_put_bulk() is
 *     "single-producer". Otherwise, it is "multi-producers".
 *   - LL_MEMPOOL_F_SC_GET: If this flag is set, the default behavior
 *     when using ll_mempool_get() or ll_mempool_get_bulk() is
 *     "single-consumer". Otherwise, it is "multi-consumers".
 *   - LL_MEMPOOL_F_NO_IOVA_CONTIG: If set, allocated objects won't
 *     necessarily be contiguous in IO memory.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with ll_errno set appropriately. Possible ll_errno values include:
 *    - E_LL_NO_CONFIG - function could not get pointer to ll_config structure
 *    - E_LL_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large or an unknown flag was passed
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct ll_mempool *
ll_mempool_create(const char *name, unsigned n, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   ll_mempool_ctor_t *mp_init, void *mp_init_arg,
		   ll_mempool_obj_cb_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags);

/**
 * Create an empty mempool
 *
 * The mempool is allocated and initialized, but it is not populated: no
 * memory is allocated for the mempool elements. The user has to call
 * ll_mempool_populate_*() to add memory chunks to the pool. Once
 * populated, the user may also want to initialize each object with
 * ll_mempool_obj_iter().
 *
 * @param name
 *   The name of the mempool.
 * @param n
 *   The maximum number of elements that can be added in the mempool.
 *   The optimum size (in terms of memory usage) for a mempool is when n
 *   is a power of two minus one: n = (2^q - 1).
 * @param elt_size
 *   The size of each element.
 * @param cache_size
 *   Size of the cache. See ll_mempool_create() for details.
 * @param private_data_size
 *   The size of the private data appended after the mempool
 *   structure. This is useful for storing some private data after the
 *   mempool structure, as is done for ll_mbuf_pool for example.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   Flags controlling the behavior of the mempool. See
 *   ll_mempool_create() for details.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with ll_errno set appropriately. See ll_mempool_create() for details.
 */
struct ll_mempool *
ll_mempool_create_empty(const char *name, unsigned n, unsigned elt_size,
	unsigned cache_size, unsigned private_data_size,
	int socket_id, unsigned flags);
/**
 * Free a mempool
 *
 * Unlink the mempool from global list, free the memory chunks, and all
 * memory referenced by the mempool. The objects must not be used by
 * other cores as they will be freed.
 *
 * @param mp
 *   A pointer to the mempool structure.
 *   If NULL then, the function does nothing.
 */
void
ll_mempool_free(struct ll_mempool *mp);

/**
 * Get a pointer to the per-lcore default mempool cache.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param lcore_id
 *   The logical core id.
 * @return
 *   A pointer to the mempool cache or NULL if disabled or unregistered non-EAL
 *   thread.
 */
static __ll_always_inline struct ll_mempool_cache *
ll_mempool_default_cache(struct ll_mempool *mp, unsigned lcore_id)
{
	if (mp->cache_size == 0)
		return NULL;

	if (lcore_id >= LL_MAX_LCORE)
		return NULL;

	return &mp->local_cache[lcore_id];
}

/**
 * Flush a user-owned mempool cache to the specified mempool.
 *
 * @param cache
 *   A pointer to the mempool cache.
 * @param mp
 *   A pointer to the mempool.
 */
static __ll_always_inline void
ll_mempool_cache_flush(struct ll_mempool_cache *cache,
			struct ll_mempool *mp)
{
	if (cache == NULL)
		cache = ll_mempool_default_cache(mp, ll_lcore_id());
	if (cache == NULL || cache->len == 0)
		return;
	ll_mempool_ops_enqueue_bulk(mp, cache->objs, cache->len);
	cache->len = 0;
}


/**
 * @internal Put several objects back in the mempool; used internally.
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to store back in the mempool, must be strictly
 *   positive.
 * @param cache
 *   A pointer to a mempool cache structure. May be NULL if not needed.
 */

static __ll_always_inline void
ll_mempool_do_generic_put(struct ll_mempool *mp, void * const *obj_table,
			   unsigned int n, struct ll_mempool_cache *cache) {
	void **cache_objs;

	/* increment stat now, adding in mempool always success */
	LL_MEMPOOL_STAT_ADD(mp, put_bulk, 1);
	LL_MEMPOOL_STAT_ADD(mp, put_objs, n);
	
	if (unlikely((cache == NULL) || (n > LL_MEMPOOL_CACHE_MAX_SIZE))) {
		goto ring_enqueue;
	}

	cache_objs = &cache->objs[cache->len];

		/* Add elements back into the cache */
	ll_memcpy(&cache_objs[0], obj_table, sizeof(void *) * n);

	cache->len += n;

	if (cache->len >= cache->flushthresh) {
		ll_mempool_ops_enqueue_bulk(mp, &cache->objs[cache->size],
				cache->len - cache->size);
		cache->len = cache->size;
	}

	return;

ring_enqueue:
	/* push remaining objects in ring */
#ifdef LL_LIBLL_MEMPOOL_DEBUG
	if (ll_mempool_ops_enqueue_bulk(mp, obj_table, n) < 0)
		ll_panic("cannot put objects in mempool\n");
#else
	ll_mempool_ops_enqueue_bulk(mp, obj_table, n);
#endif
}

/**
 * Put several objects back in the mempool.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the mempool from the obj_table.
 * @param cache
 *   A pointer to a mempool cache structure. May be NULL if not needed.
 */
static __ll_always_inline void
ll_mempool_generic_put(struct ll_mempool *mp, void * const *obj_table,
			unsigned int n, struct ll_mempool_cache *cache)
{
	LL_MEMPOOL_CHECK_COOKIES(mp, obj_table, n, 0);
	ll_mempool_do_generic_put(mp, obj_table, n, cache);
}


/**
 * Put several objects back in the mempool.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * mempool creation time (see flags).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the mempool from obj_table.
 */
static __ll_always_inline void
ll_mempool_put_bulk(struct ll_mempool *mp, void * const *obj_table,
		     unsigned int n)
{
	struct ll_mempool_cache *cache;
	cache = ll_mempool_default_cache(mp, ll_lcore_id());
	ll_mempool_generic_put(mp, obj_table, n, cache);
}


/**
 * Put one object back in the mempool.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * mempool creation time (see flags).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj
 *   A pointer to the object to be added.
 */
static __ll_always_inline void
ll_mempool_put(struct ll_mempool *mp, void *obj)
{
	ll_mempool_put_bulk(mp, &obj, 1);
}


/**
 * @internal Get several objects from the mempool; used internally.
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to get, must be strictly positive.
 * @param cache
 *   A pointer to a mempool cache structure. May be NULL if not needed.
 * @return
 *   - >=0: Success; number of objects supplied.
 *   - <0: Error; code of ring dequeue function.
 */
static __ll_always_inline int
ll_mempool_do_generic_get(struct ll_mempool *mp, void **obj_table,
			   unsigned int n, struct ll_mempool_cache *cache)
{
	int ret;
	uint32_t index, len;
	void **cache_objs;

	/* No cache provided or cannot be satisfied from cache */
	if (unlikely(cache == NULL || n >= cache->size))
		goto ring_dequeue;

	cache_objs = cache->objs;

	/* Can this be satisfied from the cache? */
	if (cache->len < n) {
		/* No. Backfill the cache first, and then fill from it */
		uint32_t req = n + (cache->size - cache->len);

		/* How many do we require i.e. number to fill the cache + the request */
		ret = ll_mempool_ops_dequeue_bulk(mp,
			&cache->objs[cache->len], req);
		if (unlikely(ret < 0)) {
			/*
			 * In the off chance that we are buffer constrained,
			 * where we are not able to allocate cache + n, go to
			 * the ring directly. If that fails, we are truly out of
			 * buffers.
			 */
			goto ring_dequeue;
		}

		cache->len += req;
	}

	/* Now fill in the response ... */
	for (index = 0, len = cache->len - 1; index < n; ++index, len--, obj_table++)
		*obj_table = cache_objs[len];

	cache->len -= n;

	LL_MEMPOOL_STAT_ADD(mp, get_success_bulk, 1);
	LL_MEMPOOL_STAT_ADD(mp, get_success_objs, n);

	return 0;

ring_dequeue:

	/* get remaining objects from ring */
	ret = ll_mempool_ops_dequeue_bulk(mp, obj_table, n);

	if (ret < 0) {
		LL_MEMPOOL_STAT_ADD(mp, get_fail_bulk, 1);
		LL_MEMPOOL_STAT_ADD(mp, get_fail_objs, n);
	} else {
		LL_MEMPOOL_STAT_ADD(mp, get_success_bulk, 1);
		LL_MEMPOOL_STAT_ADD(mp, get_success_objs, n);
	}

	return ret;
}
/**
 * Get several objects from the mempool.
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to get from mempool to obj_table.
 * @param cache
 *   A pointer to a mempool cache structure. May be NULL if not needed.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static __ll_always_inline int
ll_mempool_generic_get(struct ll_mempool *mp, void **obj_table,
			unsigned int n, struct ll_mempool_cache *cache)
{
	int ret;
	ret = ll_mempool_do_generic_get(mp, obj_table, n, cache);
	if (ret == 0)
		LL_MEMPOOL_CHECK_COOKIES(mp, obj_table, n, 1);
	return ret;
}

/**
 * Get several objects from the mempool.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * mempool creation time (see flags).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to get from the mempool to obj_table.
 * @return
 *   - 0: Success; objects taken
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static __ll_always_inline int
ll_mempool_get_bulk(struct ll_mempool *mp, void **obj_table, unsigned int n)
{
	struct ll_mempool_cache *cache;
	cache = ll_mempool_default_cache(mp, ll_lcore_id());
	return ll_mempool_generic_get(mp, obj_table, n, cache);
}

/**
 * Get one object from the mempool.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behavior that was specified at
 * mempool creation (see flags).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static __ll_always_inline int
ll_mempool_get(struct ll_mempool *mp, void **obj_p)
{
	return ll_mempool_get_bulk(mp, obj_p, 1);
}



#ifdef __cplusplus
}
#endif
#endif	//_LL_MEMPOOL_H_
