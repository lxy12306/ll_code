#ifndef __LL_RING_GENERIC_PVT_H_
#define __LL_RING_GENERIC_PVT_H_

#include <ll_pause.h>

static __ll_always_inline void
__ll_ring_update_tail(struct ll_ring_headtail *ht, uint32_t old_val,
			uint32_t new_val, uint32_t single, uint32_t enqueue) {
	if (enqueue)
	  ll_smp_wmb();
	else
	  ll_smp_rmb();
	
	if (!single) {
			ll_wait_until_equal_32(&ht->tail, old_val, __ATOMIC_RELAXED);	
	}
	ht->tail = new_val;
}

/**
 * @internal This function updates the producer head for senqueue
 */
static __ll_always_inline unsigned int
__ll_ring_move_prod_head(struct ll_ring *r, unsigned int is_sp,
			unsigned int n, enum ll_ring_queue_behavior behavior,
			uint32_t *old_head, uint32_t *new_head,
			uint32_t *entries) {
	unsigned int max = n;
	int success;
	
	do {
		/*consider in one loop, cons some food*/
		n = max;
		
		*old_head = r->prod.head;

		ll_smp_rmb();
		
		/**
		 * cons_tail < *old_head
		 * */
		*entries = (r->capacity + r->cons.tail - *old_head);
		
		if (n > *entries) {
			n = (behavior == LL_RING_QUEUE_FIXED)? 0: *entries;
		}
		
		if (unlikely(n == 0)) {
			return 0;
		}
		
		*new_head = *old_head + n;
		if (is_sp) {
			r->prod.head = *new_head;
			success = 1; //just one prob, do not need to consider sync
		} else {
			success = ll_atomic32_cmpset(&r->prod.head, *old_head, *new_head); //make sure old stauts consistence
		}
	
	}while(unlikely(success == 0));

	return n;
}

/**
 * @internal This function updates the consumer head for senqueue
 */
static __ll_always_inline unsigned int
__ll_ring_move_cons_head(struct ll_ring *r, unsigned int is_sc,
			unsigned int n, enum ll_ring_queue_behavior behavior,
			uint32_t *old_head, uint32_t *new_head,
			uint32_t *entries) {
	unsigned int max = n;
	int success;
	
	do {
		/*consider in one loop, cons some food*/
		n = max;
		
		*old_head = r->cons.head;

		ll_smp_rmb();
		
		/**
		 * cons_tail < *old_head
		 * */
		*entries = (r->prod.tail - *old_head);
		
		if (n > *entries) {
			n = (behavior == LL_RING_QUEUE_FIXED)? 0: *entries;
		}
		
		if (unlikely(n == 0)) {
			return 0;
		}
		
		*new_head = *old_head + n;
		if (is_sc) {
			r->cons.head = *new_head;
			success = 1; //just one prob, do not need to consider sync
		} else {
			success = ll_atomic32_cmpset(&r->cons.head, *old_head, *new_head); //make sure old stauts consistence
		}
	
	}while(unlikely(success == 0));

	return n;
}

#endif //__LL_RING_GENERIC_PVT_H_
