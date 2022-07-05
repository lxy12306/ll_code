#ifndef MALLOC_ELEM_H_
#define MALLOC_ELEM_H_

#include <stdbool.h>

#include <ll_config.h>

#define LL_MALLOC_DEBUG 1
#define LL_MALLOC_ASAN 1

#define MIN_DATA_SIZE (LL_CACHE_LINE_SIZE)

/* dummy definition of struct so we can use pointers to it in malloc_elem struct */
struct malloc_heap;

enum elem_state {
	ELEM_FREE = 0,
	ELEM_BUSY,
	ELEM_PAD,  /* element is a padding-only header ? */
};

struct malloc_elem {
    struct malloc_heap *heap;

	struct malloc_elem *volatile prev;
    /**< points to prev elem in memseg */
	struct malloc_elem *volatile next;
	/**< points to next elem in memseg */
	LIST_ENTRY(malloc_elem) free_list;
	/**< list of free elements in heap */

	struct ll_memseg_list *msl;

    enum elem_state state : 3;
	/** If state == ELEM_FREE: the memory is not filled with zeroes. */
	uint32_t dirty : 1;
	/** Reserved for future use. */
	uint32_t reserved : 28;
	uint32_t pad;
	size_t size;
	struct malloc_elem *orig_elem;
	size_t orig_size;

#ifdef LL_MALLOC_DEBUG
	uint64_t header_cookie;         /* Cookie marking start of data */
	                                /* trailer cookie at start + size */
#endif
#ifdef LL_MALLOC_ASAN
	size_t user_size;
	uint64_t asan_cookie[2]; /* must be next to header_cookie */
#endif
}__ll_cache_aligned;

static const unsigned int MALLOC_ELEM_HEADER_LEN = sizeof(struct malloc_elem);


#ifndef LL_MALLOC_DEBUG
#ifdef LL_MALLOC_ASAN
static const unsigned int MALLOC_ELEM_TRAILER_LEN = LL_CACHE_LINE_SIZE;
#else
static const unsigned int MALLOC_ELEM_TRAILER_LEN;
#endif


/* dummy function - just check if pointer is non-null */
static inline int
malloc_elem_cookies_ok(const struct malloc_elem *elem){ return elem != NULL; }

/* dummy function - no header if malloc_debug is not enabled */
static inline void
set_header(struct malloc_elem *elem __ll_unused){ }

/* dummy function - no trailer if malloc_debug is not enabled */
static inline void
set_trailer(struct malloc_elem *elem __ll_unused){ }

#else
static const unsigned int MALLOC_ELEM_TRAILER_LEN = LL_CACHE_LINE_SIZE;

#define MALLOC_HEADER_COOKIE   0xbadbadbadadd2e55ULL /**< Header cookie. */
#define MALLOC_TRAILER_COOKIE  0xadd2e55badbadbadULL /**< Trailer cookie.*/

/* define macros to make referencing the header and trailer cookies easier */
#define MALLOC_ELEM_TRAILER(elem) (*((uint64_t*)LL_PTR_ADD(elem, \
		elem->size - MALLOC_ELEM_TRAILER_LEN)))
#define MALLOC_ELEM_HEADER(elem) (elem->header_cookie)

static inline void
set_header(struct malloc_elem *elem)
{
	if (elem != NULL)
		MALLOC_ELEM_HEADER(elem) = MALLOC_HEADER_COOKIE;
}

static inline void
set_trailer(struct malloc_elem *elem)
{
	if (elem != NULL)
		MALLOC_ELEM_TRAILER(elem) = MALLOC_TRAILER_COOKIE;
}

/* check that the header and trailer cookies are set correctly */
static inline int
malloc_elem_cookies_ok(const struct malloc_elem *elem)
{
	return elem != NULL &&
			MALLOC_ELEM_HEADER(elem) == MALLOC_HEADER_COOKIE &&
			MALLOC_ELEM_TRAILER(elem) == MALLOC_TRAILER_COOKIE;
}
#endif

#define MALLOC_ELEM_OVERHEAD (MALLOC_ELEM_HEADER_LEN + MALLOC_ELEM_TRAILER_LEN)

#ifdef LL_MALLOC_ASAN

/*
 * ASAN_SHADOW_OFFSET should match to the corresponding
 * value defined in gcc/libsanitizer/asan/asan_mapping.h
 */
#ifdef LL_ARCH_X86_64
#define ASAN_SHADOW_OFFSET    0x00007fff8000
#elif defined(LL_ARCH_ARM64)
#define ASAN_SHADOW_OFFSET    0x001000000000
#elif defined(LL_ARCH_PPC_64)
#define ASAN_SHADOW_OFFSET    0x020000000000
#endif

#define ASAN_SHADOW_GRAIN_SIZE	8
#define ASAN_MEM_FREE_FLAG	0xfd
#define ASAN_MEM_REDZONE_FLAG	0xfa
#define ASAN_SHADOW_SCALE    3

#define ASAN_MEM_SHIFT(mem) ((void*)((uintptr_t)(mem) >> ASAN_SHADOW_SCALE))
#define ASAN_MEM_TO_SHADOW(mem) \
	LL_PTR_ADD(ASAN_MEM_SHIFT(mem), ASAN_SHADOW_OFFSET)

#if defined(__clang__)
#define __ll_no_asan __attribute__((no_sanitize("address", "hwaddress")))
#else
#define __ll_no_asan __attribute__((no_sanitize_address))
#endif

__ll_no_asan
static inline void
asan_set_shadow(void *addr, char val) {
	*(char *)addr = val;
}

static inline void
asan_set_freezone(void *ptr __ll_unused, size_t size __ll_unused) { }

static inline void
asan_clear_alloczone(struct malloc_elem *elem __ll_unused) { }

static inline void
asan_clear_split_alloczone(struct malloc_elem *elem __ll_unused) { }

static inline void
asan_set_redzone(struct malloc_elem *elem __ll_unused,
					size_t user_size __ll_unused) { }

static inline void
asan_clear_redzone(struct malloc_elem *elem __ll_unused) { }

static inline size_t
old_malloc_size(struct malloc_elem *elem)
{
	return elem->size - elem->pad - MALLOC_ELEM_OVERHEAD;
}

#else /* !LL_MALLOC_ASAN */

#define __rte_no_asan

static inline void
asan_set_freezone(void *ptr __ll_unused, size_t size __ll_unused) { }

static inline void
asan_clear_alloczone(struct malloc_elem *elem __ll_unused) { }

static inline void
asan_clear_split_alloczone(struct malloc_elem *elem __ll_unused) { }

static inline void
asan_set_redzone(struct malloc_elem *elem __ll_unused,
					size_t user_size __ll_unused) { }

static inline void
asan_clear_redzone(struct malloc_elem *elem __ll_unused) { }

static inline size_t
old_malloc_size(struct malloc_elem *elem)
{
	return elem->size - elem->pad - MALLOC_ELEM_OVERHEAD;
}
#endif /* !LL_MALLOC_ASAN */

/*
 * Given a pointer to the start of a memory block returned by malloc, get
 * the actual malloc_elem header for that block.
 */
static inline struct malloc_elem *
malloc_elem_from_data(const void *data)
{
	if (data == NULL)
		return NULL;

	struct malloc_elem *elem = LL_PTR_SUB(data, MALLOC_ELEM_HEADER_LEN);
	if (!malloc_elem_cookies_ok(elem))
		return NULL;
	return elem->state != ELEM_PAD ? elem:  LL_PTR_SUB(elem, elem->pad);
}



/*
 * initialise a malloc_elem header
 */
void
malloc_elem_init(struct malloc_elem *elem,
		struct malloc_heap *heap,
		struct ll_memseg_list *msl,
		size_t size,
		struct malloc_elem *orig_elem,
		size_t orig_size,
		bool dirty);

void
malloc_elem_insert(struct malloc_elem *elem);

/*
 * return true if the current malloc_elem can hold a block of data
 * of the requested size and with the requested alignment
 */
int
malloc_elem_can_hold(struct malloc_elem *elem, size_t size,
		unsigned int align, size_t bound, bool contig);

/*
 * reserve a block of data in an existing malloc_elem. If the malloc_elem
 * is much larger than the data block requested, we split the element in two.
 */
struct malloc_elem *
malloc_elem_alloc(struct malloc_elem *elem, size_t size,
		unsigned int align, size_t bound, bool contig);

/*
 * free a malloc_elem block by adding it to the free list. If the
 * blocks either immediately before or immediately after newly freed block
 * are also free, the blocks are merged together.
 */
struct malloc_elem *
malloc_elem_free(struct malloc_elem *elem);

struct malloc_elem *
malloc_elem_join_adjacent_free(struct malloc_elem *elem);

/*
 * attempt to resize a malloc_elem by expanding into any free space
 * immediately after it in memory.
 */
int
malloc_elem_resize(struct malloc_elem *elem, size_t size);

void
malloc_elem_hide_region(struct malloc_elem *elem, void *start, size_t len);

void
malloc_elem_free_list_remove(struct malloc_elem *elem);

/*
 * dump contents of malloc elem to a file.
 */
void
malloc_elem_dump(const struct malloc_elem *elem, FILE *f);

/*
 * Given an element size, compute its freelist index.
 */
size_t
malloc_elem_free_list_index(size_t size);

/*
 * Add element to its heap's free list.
 */
void
malloc_elem_free_list_insert(struct malloc_elem *elem);

/*
 * Find biggest IOVA-contiguous zone within an element with specified alignment.
 */
size_t
malloc_elem_find_max_iova_contig(struct malloc_elem *elem, size_t align);



#endif //MALLOC_ELEM_H_