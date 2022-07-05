#ifndef _LL_RING_RTS_ELEM_PVT_H_
#define _LL_RING_RTS_ELEM_PVT_H_

/**
 * @internal This function updates tail values
 */
static __ll_always_inline void
__ll_ring_rts_update_tail(struct ll_ring_rts_headtail *ht) {
	
	union __ll_ring_rts_poscnt h, ot, nt;
	
	ot.raw =  __atomic_load_n(&ht->tail.raw, __ATOMIC_ACQUIRE);

	do {
		h.raw = __atomic_load_n(&ht->head.raw, __ATOMIC_RELAXED);
		
		nt.raw = ot.raw;
		if (++nt.val.cnt == h.val.cnt) {
			nt.val.pos = h.val.pos;
		}
		
	} while (__atomic_compare_exchange_n(&ht->tail.raw, &ot.raw, nt.raw,
			0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE) == 0);
}


/**
 * @internal This function waits till head/tail distance wouldn't
 * exceed pre-defined max valuei
 * why do we need a distance ? this is to prevent the consumer(producer) from
 * occupying the ring until the ring is empty(full), so that the producer(consumer) can
 * produce (consume) successfully
 */
static __ll_always_inline void
__ll_ring_rts_head_wait(const struct ll_ring_rts_headtail *ht,
	union __ll_ring_rts_poscnt *h) {
	
	uint32_t max;
	
	max = ht->htd_max;

	//in here,not use atomic read for ht->tail.val because tail will not change in change head
	while ((h->val.pos - ht->tail.val.pos) > max) {
		ll_pause();
		h->raw = __atomic_load_n(&ht->head.raw,__ATOMIC_ACQUIRE);
	}
}

/**
 * @internal This function updates the producer head for enqueue
 */
static __ll_always_inline uint32_t
__ll_ring_rts_move_prod_head(struct ll_ring *r, uint32_t num,
	enum ll_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *free_entries) {
	uint32_t n;
	const uint32_t capacity = r->capacity;
	union __ll_ring_rts_poscnt oh, nh;
	
	oh.raw = __atomic_load_n(&r->rts_prod.head.raw,__ATOMIC_ACQUIRE);

	do {
		//Reset n to the initial burst count
		n = num;

		// make sure that we read prod head *before* reading cons tail
		__ll_ring_rts_head_wait(&r->rts_prod, &oh);
		
		*free_entries = capacity + r->cons.tail - oh.val.pos;

		//set the actual entries for enqueue
		if (unlikely(n > *free_entries)) {
			n = (behavior == LL_RING_QUEUE_FIXED) ? 0: *free_entries;
		}

		if (n == 0) {
			break;
		}

		nh.val.pos = oh.val.pos + n;
		nh.val.cnt = oh.val.cnt + 1;


	} while(__atomic_compare_exchange_n(&r->rts_prod.head.raw,
			&oh.raw, nh.raw, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE) == 0);
	//just like normal ring
	*old_head = oh.val.pos;
	
	return n;
}

/**
 * @internal This function updates the consumer head for dequeue
 */
static __ll_always_inline uint32_t
__ll_ring_rts_move_cons_head(struct ll_ring *r, uint32_t num,
	enum ll_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries) {
	uint32_t n;
	union __ll_ring_rts_poscnt nh, oh;
	
	oh.raw = __atomic_load_n(&r->rts_cons.head.raw, __ATOMIC_ACQUIRE);
	
	//move cons.head atomically
	do {
		//Reset n to the initial burst count
		n = num;

		/**
		 * wait for cons head/tail distance
		 * make sure that we read cons head *before*
		 * reading  prod tail
		 */
		__ll_ring_rts_head_wait(&r->rts_cons, &oh);

		*entries = r->prod.tail - oh.val.pos;
		
		//set the actual entries for dequeue
		if (n > *entries) {
			n = (behavior == LL_RING_QUEUE_FIXED) ? 0: *entries;
		}
		
		if (unlikely(n == 0)) {
			break;
		}

		LL_LOG(INFO, RING, "oh.val.pos,oh.val.cnt :<%d, %d> \n", oh.val.pos, oh.val.cnt);
		nh.val.pos = oh.val.pos + n;
		nh.val.cnt = oh.val.cnt + 1;

	} while(__atomic_compare_exchange_n(&r->rts_cons.head.raw,
			&oh.raw, nh.raw, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE) == 0);
	
	LL_LOG(INFO, RING, "out-------oh.val.pos,oh.val.cnt :<%d, %d> \n", oh.val.pos, oh.val.cnt);
	*old_head = oh.val.pos;
	return n;
}

/**
 * @internal Enqueue several objects on the RTS ring
 * 
 * @param r
 *  A 
 *
 */
static __ll_always_inline unsigned int
__ll_ring_do_rts_enqueue_elem(struct ll_ring *r, const void *obj_table,
	uint32_t esize, uint32_t n, enum ll_ring_queue_behavior behavior,
	uint32_t *free_space) {
	
	uint32_t free, head;
	
	n = __ll_ring_rts_move_prod_head(r, n, behavior, &head, &free);
	
	if (n != 0) {
		__ll_ring_enqueue_elems(r, head, obj_table, esize, n);
		__ll_ring_rts_update_tail(&r->rts_prod);
	}

	if (free_space != NULL) {
		*free_space = free - n;
	}
	
	return n;
}


static __ll_always_inline unsigned int
__ll_ring_do_rts_dequeue_elem(struct ll_ring *r, void *obj_table,
	uint32_t esize, uint32_t n, enum ll_ring_queue_behavior behavior,
	uint32_t *available) {
	
	uint32_t entries, head;
	
	n = __ll_ring_rts_move_cons_head(r, n, behavior, &head, &entries);

	LL_LOG(INFO, RING, "head,n :<%d, %d> \n", head, n);
	
	if (n != 0) {
		__ll_ring_dequeue_elems(r, head, obj_table, esize, n);
		__ll_ring_rts_update_tail(&r->rts_cons);
	}

	if (available != NULL) {
		*available = entries - n;
	}

	return n;
}

#endif //_LL_RING_RTS_ELEM_PVT_H_
