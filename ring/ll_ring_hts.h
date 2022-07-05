#ifndef _LL_RING_HTS_H_
#define _LL_RING_HTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ll_ring_hts_elem_pvt.h"

/**
 * Enqueue several objects on the HTS ring (multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __ll_always_inline unsigned int
ll_ring_mp_hts_enqueue_bulk_elem(struct ll_ring *r, const void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __ll_ring_do_hts_enqueue_elem(r, obj_table, esize, n,
			LL_RING_QUEUE_FIXED, free_space);
}

/**
 * Dequeue several objects from an HTS ring (multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __ll_always_inline unsigned int
ll_ring_mc_hts_dequeue_bulk_elem(struct ll_ring *r, void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *available)
{
	return __ll_ring_do_hts_dequeue_elem(r, obj_table, esize, n,
		LL_RING_QUEUE_FIXED, available);
}

/**
 * Enqueue several objects on the HTS ring (multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static __ll_always_inline unsigned int
ll_ring_mp_hts_enqueue_burst_elem(struct ll_ring *r, const void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __ll_ring_do_hts_enqueue_elem(r, obj_table, esize, n,
			LL_RING_QUEUE_VARIABLE, free_space);
}

/**
 * Dequeue several objects from an HTS  ring (multi-consumers safe).
 * When the requested objects are more than the available objects,
 * only dequeue the actual number of objects.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static __ll_always_inline unsigned int
ll_ring_mc_hts_dequeue_burst_elem(struct ll_ring *r, void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *available)
{
	return __ll_ring_do_hts_dequeue_elem(r, obj_table, esize, n,
			LL_RING_QUEUE_VARIABLE, available);
}

/**
 * Enqueue several objects on the HTS ring (multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __ll_always_inline unsigned int
ll_ring_mp_hts_enqueue_bulk(struct ll_ring *r, void * const *obj_table,
			 unsigned int n, unsigned int *free_space)
{
	return ll_ring_mp_hts_enqueue_bulk_elem(r, obj_table,
			sizeof(uintptr_t), n, free_space);
}

/**
 * Dequeue several objects from an HTS ring (multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __ll_always_inline unsigned int
ll_ring_mc_hts_dequeue_bulk(struct ll_ring *r, void **obj_table,
		unsigned int n, unsigned int *available)
{
	return ll_ring_mc_hts_dequeue_bulk_elem(r, obj_table,
			sizeof(uintptr_t), n, available);
}

/**
 * Enqueue several objects on the HTS ring (multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static __ll_always_inline unsigned int
ll_ring_mp_hts_enqueue_burst(struct ll_ring *r, void * const *obj_table,
			 unsigned int n, unsigned int *free_space)
{
	return ll_ring_mp_hts_enqueue_burst_elem(r, obj_table,
			sizeof(uintptr_t), n, free_space);
}

/**
 * Dequeue several objects from an HTS  ring (multi-consumers safe).
 * When the requested objects are more than the available objects,
 * only dequeue the actual number of objects.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static __ll_always_inline unsigned int
ll_ring_mc_hts_dequeue_burst(struct ll_ring *r, void **obj_table,
		unsigned int n, unsigned int *available)
{
	return ll_ring_mc_hts_dequeue_burst_elem(r, obj_table,
			sizeof(uintptr_t), n, available);
}

#ifdef __cplusplus
}
#endif

#endif //_LL_RING_HTS_H_
