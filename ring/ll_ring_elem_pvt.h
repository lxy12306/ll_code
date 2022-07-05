#ifndef _LL_RING_ELEM_PVT_H_
#define _LL_RING_ELEM_PVT_H_

#include <ll_common.h>
#include "ll_ring_core.h"

static __ll_always_inline void
__ll_ring_enqueue_elems_32(struct ll_ring *r, const uint32_t size,
			uint32_t idx, const void *obj_table, uint32_t n) {
	unsigned int i;
	uint32_t *ring = (uint32_t *)&r[1];
	uint32_t *obj = (uint32_t *)obj_table;
	
	if (likely(idx + n <= size)) {
		for (i = 0; i < (n &(~0x7)); i+=8, idx+=8) {
			ring[idx] = obj[i];
			ring[idx + 1] = obj[i + 1];
			ring[idx + 2] = obj[i + 2];
			ring[idx + 3] = obj[i + 3];
			ring[idx + 4] = obj[i + 4];
			ring[idx + 5] = obj[i + 5];
			ring[idx + 6] = obj[i + 6];
			ring[idx + 7] = obj[i + 7];
		}
		switch (n & 0x7) {
			case 7:
				ring[idx++] = obj[i++];
			case 6:
				ring[idx++] = obj[i++];
			case 5:
				ring[idx++] = obj[i++];
			case 4:
				ring[idx++] = obj[i++];
			case 3:
				ring[idx++] = obj[i++];
			case 2:
				ring[idx++] = obj[i++];
			case 1:
				ring[idx++] = obj[i++];
		}
	} else {
		for (i = 0; idx < size; ++i, ++idx) {
			ring[idx] = obj[i];
		}
		for (idx = 0; i < n; ++i, ++idx) {
			ring[idx] = obj[i];
		}
	}
}

static __ll_always_inline void
__ll_ring_enqueue_elems_64(struct ll_ring *r, uint32_t prod_head,
			const void *obj_table, uint32_t n) {
	const uint32_t size = r->size;
	uint32_t idx = prod_head & r->mask;
	uint64_t *ring = (uint64_t*)&r[1];
	unsigned int i = 0;
	unaligned_uint64_t *obj = (unaligned_uint64_t *)obj_table;
	
	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & (~0x3)); i+=4, idx+=4) {
			ring[idx] = obj[i];
			ring[idx + 1] = obj[i + 1];
			ring[idx + 2] = obj[i + 2];
			ring[idx + 3] = obj[i + 3];
		}
		
		switch(n & 0x3) {
			case 3:
				ring[idx++] = obj[i++]; 
			case 2:
				ring[idx++] = obj[i++]; 
			case 1:
				ring[idx++] = obj[i++]; 
		}
	} else { /*rewind processing*/
		for (i = 0; idx < size; ++i, ++idx) {
			ring[idx] = obj[i];
		}
		for (idx = 0; i < n; ++i, ++idx) {	
			ring[idx] = obj[i];
		}
	}
}

/**
 * the actual enqueue of elements on the ring.
 * Placed here since identical code needed in both
 * single and multi producer enqueue functions
 */
static __ll_always_inline void
__ll_ring_enqueue_elems(struct ll_ring *r, uint32_t prod_head,
			const void *obj_table, uint32_t esize, uint32_t num) {
	if (esize == 8) {
		__ll_ring_enqueue_elems_64(r, prod_head, obj_table, num);
	} else {
		uint32_t idx, scale, nr_idx, nr_num, nr_size;

		/*Normalize to uint32_t*/
		scale = esize / sizeof(uint32_t);
		idx = prod_head & r->mask;
		nr_num = num * scale;
		nr_size = r->size * scale;		
		nr_idx = idx * scale;
		__ll_ring_enqueue_elems_32(r, nr_size, nr_idx, obj_table, nr_num);
	}
}

static __ll_always_inline void
__ll_ring_dequeue_elems_32(struct ll_ring *r, const uint32_t size,
	uint32_t idx, void *obj_table, uint32_t n) {

	unsigned int i;
	uint32_t *ring = (uint32_t *)&r[1];
	uint32_t *obj = (uint32_t *)obj_table;
	
	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & (~0x7)); i+=8, idx+=8) {
			obj[i] = ring[idx];
			obj[i + 1] = ring[idx + 1];
			obj[i + 2] = ring[idx + 2];
			obj[i + 3] = ring[idx + 3];
			obj[i + 4] = ring[idx + 4];
			obj[i + 5] = ring[idx + 5];
			obj[i + 6] = ring[idx + 6];
			obj[i + 7] = ring[idx + 7];
		}
		switch (n & 0x7) {
			case 7:
				obj[i++] = ring[idx++];
			case 6:
				obj[i++] = ring[idx++];
			case 5:
				obj[i++] = ring[idx++];
			case 4:
				obj[i++] = ring[idx++];
			case 3:
				obj[i++] = ring[idx++];
			case 2:
				obj[i++] = ring[idx++];
			case 1:
				obj[i++] = ring[idx++];
		}
	} else {
		for (i = 0; idx < size; ++i, ++idx) {
			obj[i] = ring[idx]; 
		}
		for (idx = 0; i < n; ++i, ++idx) {
			obj[i] = ring[idx]; 
		}
	}
}

static __ll_always_inline void
__ll_ring_dequeue_elems_64(struct ll_ring *r, uint32_t cons_head,
	void *obj_table, uint32_t n) {

	const uint32_t size = r->size;
	uint32_t idx = cons_head & r->mask;
	uint64_t *ring = (uint64_t*)&r[1];
	unsigned int i = 0;
	unaligned_uint64_t *obj = (unaligned_uint64_t *)obj_table;

#if __ll_ring_dequeue_elems_64_0
	LL_LOG(INFO, RING, "dequeue :<%d, %d> \n", cons_head, n);
#endif
	
	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & (~0x3)); i+=4, idx+=4) {
			obj[i] = ring[idx];
			obj[i + 1] = ring[idx + 1];
			obj[i + 2] = ring[idx + 2];
			obj[i + 3] = ring[idx + 3];

#if __ll_ring_dequeue_elems_64_0
			LL_LOG(INFO, RING, "dequeue :<%p, %p, %p, %p> \n", obj[i],obj[i + 1],obj[i + 2],obj[i + 3]);
#endif
		}
		
		switch(n & 0x3) {
			case 3:
				obj[i++] = ring[idx++];
#if __ll_ring_dequeue_elems_64_0
				LL_LOG(INFO, RING, "dequeue :<%p> \n", obj[i - 1]);
#endif	
			case 2:
				obj[i++] = ring[idx++];
#if __ll_ring_dequeue_elems_64_0
			LL_LOG(INFO, RING, "dequeue :<%p> \n", obj[i - 1]);
#endif
			case 1:
				obj[i++] = ring[idx++];
#if __ll_ring_dequeue_elems_64_0
				LL_LOG(INFO, RING, "dequeue :<%p> \n", obj[i - 1]);
#endif
		}
	} else { /*rewind processing*/
		for (i = 0; idx < size; ++i, ++idx) {
			obj[i] = ring[idx];
				LL_LOG(INFO, RING, "rewind dequeue :<%p> \n", obj[i]);
		}
		for (idx = 0; i < n; ++i, ++idx) {	
			obj[i] = ring[idx];
				LL_LOG(INFO, RING, "rewind dequeue :<%p> \n", obj[i]);
		}
	}
}

/**
 * the actual dequeue of elements on the ring.
 * Placed here since identical code needed in both
 * single and multi consumer dequeue functions
 */
static __ll_always_inline void
__ll_ring_dequeue_elems(struct ll_ring *r, uint32_t cons_head,
			void *obj_table, uint32_t esize, uint32_t num) {
	if (esize == 8) {
		__ll_ring_dequeue_elems_64(r, cons_head, obj_table, num);
	} else {
		uint32_t idx, scale, nr_idx, nr_num, nr_size;

		/*Normalize to uint32_t*/
		scale = esize / sizeof(uint32_t);
		idx = cons_head & r->mask;
		nr_num = num * scale;
		nr_size = r->size * scale;		
		nr_idx = idx * scale;
		__ll_ring_dequeue_elems_32(r, nr_size, nr_idx, obj_table, nr_num);
	}
}

#include "ll_ring_generic_pvt.h"

/**
 * @internal Enqueue several objects on the ring
 *
 * @param r
 *	A pointer to the ring structure.
 *
 * @param obj_table
 *	A pointer to a table of objects.
 *
 * @param esize
 *  The size of ring element, in bytes. It must be a multiple of 4.
 *  This must be the same value used while creating the ring. Otherwise
 *  the results are undefined
 *
 * @param n
 *	The number of objects to add in the ring from the obj_table.
 *
 * @param behavior
 *	LL_RING_QUEUE_FIXED: Dequeue a fixed number of items from a ring
 *	LL_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 *
 * @param is_sp:
 *	Indicates whether to use single producer or multi-producer head update
 *
 * @param available
 *  returns the amount of space after the enqueue operation has finished
 *
 * @return
 *  - Actual number of objects enqueued.
 *   If behavior == LL_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __ll_always_inline unsigned int
__ll_ring_do_enqueue_elem(struct ll_ring *r, const void *obj_table,
			unsigned int esize, unsigned int n,
			enum ll_ring_queue_behavior behavior, unsigned int is_sp,
			unsigned int *free_space) {
	uint32_t prod_head, prod_next;
	uint32_t free_entries;

	n = __ll_ring_move_prod_head(r, is_sp, n, behavior,
				&prod_head, &prod_next, &free_entries);
	if (n == 0) {
		goto end;
	}

	__ll_ring_enqueue_elems(r, prod_head, obj_table, esize, n);

	__ll_ring_update_tail(&r->prod, prod_head, prod_next, is_sp, 1);

end:
	if (free_space != NULL) {
		*free_space = free_entries - n;
	}
	return n;
}
/**
 * @internal Dequeue several objects from the ring
 *
 * @param r
 *	A pointer to the ring structure.
 *
 * @param obj_table
 *	A pointer to a table of objects.
 *
 * @param esize
 *  The size of ring element, in bytes. It must be a multiple of 4.
 *  This must be the same value used while creating the ring. Otherwise
 *  the results are undefined
 *
 * @param n
 *	The number of objects to pull from the ring.
 *
 * @param behavior
 *	LL_RING_QUEUE_FIXED: Dequeue a fixed number of items from a ring
 *	LL_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 *
 * @param is_sc
 *	Indicates whether multi-consumer path is needed or not
 *
 * @param available
 *  return the number of remaining ring entries after the dequeue has finished
 *
 * @return
 *  - Actual number of objects dequeueed.
 *   If behavior == LL_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __ll_always_inline unsigned int
__ll_ring_do_dequeue_elem(struct ll_ring *r, void *obj_table,
			unsigned int esize, unsigned int n,
			enum ll_ring_queue_behavior behavior, unsigned int is_sc,
			unsigned int *available) {
	uint32_t cons_head, cons_next;
	uint32_t entries;
	
	n = __ll_ring_move_cons_head(r, is_sc, n, behavior,
				&cons_head, &cons_next, &entries);
	if (unlikely(n == 0)) {
		goto end;
	}

	__ll_ring_dequeue_elems(r, cons_head, obj_table, esize, n);

	__ll_ring_update_tail(&r->cons, cons_head, cons_next, is_sc, 1);
end:
	if (available != NULL) {
		*available = entries - n;
	}
	return n;
}

#endif //_LL_RING_ELEM_PVT_H_
