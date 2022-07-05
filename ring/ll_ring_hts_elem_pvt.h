#ifndef _LL_RING_HTS_ELEM_PVT_H_
#define _LL_RING_HTS_ELEM_PVT_H_

/**
 * @internal update tail with new value
 */
static __ll_always_inline void
__ll_ring_hts_update_tail(struct ll_ring_hts_headtail *ht, uint32_t old_tail, uint32_t num, uint32_t enqueue) {
	uint32_t tail;
	
	LL_SET_USED(enqueue);
	
	tail = old_tail + num;

	__atomic_store_n(&ht->ht.pos.tail, tail, __ATOMIC_RELEASE);
}

/**
 * @internal waits till tail will become equal to head.
 * Means no writer/reader is active for that ring.
 * Suppose to work as serialization point
 */
static __ll_always_inline void
__ll_ring_hts_head_wait(const struct ll_ring_hts_headtail *ht, union __ll_ring_hts_pos *p) {
	while (p->pos.head != p->pos.tail) {
		ll_pause();
		p->raw = __atomic_load_n(&ht->ht.raw, __ATOMIC_ACQUIRE);
	}
}

/**
 * @internal This function updates the producer head for enqueue
 */
static __ll_always_inline unsigned int
__ll_ring_hts_move_prod_head(struct ll_ring *r, unsigned int num,
	enum ll_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *free_entries) {
	uint32_t n;
	union __ll_ring_hts_pos np, op;
	
	const uint32_t capacity = r->capacity;
	
	op.raw = __atomic_load_n(&r->hts_prod.ht.raw, __ATOMIC_ACQUIRE);
	
	do {
		n = num;
		
		__ll_ring_hts_head_wait(&r->hts_prod, &op);
		
		*free_entries = capacity + r->cons.tail - op.pos.head;
		

		if (unlikely(n > *free_entries)) {
			n = (behavior == LL_RING_QUEUE_FIXED) ? 0 : *free_entries;
		}
		
		if (n == 0) {
			break;
		}

		np.pos.tail = op.pos.tail;
		np.pos.head = op.pos.head + n;


	} while(__atomic_compare_exchange_n(&r->hts_prod.ht.raw, &op.raw, &np.raw, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE));
	
	*old_head = op.pos.head;

	return n;
}

/**
 * @internal This function updates the consumer head for enqueue
 */
static __ll_always_inline unsigned int
__ll_ring_hts_move_cons_head(struct ll_ring *r, unsigned int num,
	enum ll_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries) {
	uint32_t n;
	union __ll_ring_hts_pos np, op;
	
	op.raw = __atomic_load_n(&r->hts_cons.ht.raw, __ATOMIC_ACQUIRE);
	
	do {
		n = num;
		
		__ll_ring_hts_head_wait(&r->hts_cons, &op);
		
		*entries = r->prod.tail - op.pos.head;
		

		if (unlikely(n > *entries)) {
			n = (behavior == LL_RING_QUEUE_FIXED) ? 0 : *entries;
		}
		
		if (n == 0) {
			break;
		}

		np.pos.tail = op.pos.tail;
		np.pos.head = op.pos.head + n;


	} while(__atomic_compare_exchange_n(&r->hts_cons.ht.raw, &op.raw, &np.raw, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE));
	
	*old_head = op.pos.head;

	return n;
}

static __ll_always_inline unsigned int
__ll_ring_do_hts_enqueue_elem(struct ll_ring *r, const void *obj_table,
	uint32_t esize, uint32_t n, enum ll_ring_queue_behavior behavior,
	uint32_t *free_space) {
	uint32_t free, head;
	
	n = __ll_ring_hts_move_prod_head(r, n, behavior, &head, &free);
	
	if (n != 0) {
		__ll_ring_enqueue_elems(r, head, obj_table, esize, n);
		__ll_ring_hts_update_tail(&r->hts_prod, head, n, 1);
	}

	if (free_space != NULL) {
		*free_space = free - n;
	}

	return n;
}

static __ll_always_inline unsigned int
__ll_ring_do_hts_dequeue_elem(struct ll_ring *r, void *obj_table,
	uint32_t esize, uint32_t n, enum ll_ring_queue_behavior behavior,
	uint32_t *available) {
	
	uint32_t entries, head;
	
	n = __ll_ring_hts_move_cons_head(r, n, behavior, &head, &entries);
	
	if (n != 0) {
		__ll_ring_dequeue_elems(r, head, obj_table, esize, n);
		__ll_ring_hts_update_tail(&r->hts_cons, head, n, 0);
	}

	if (available != NULL) {
		*available = entries - n;
	}

	return n;
}


#endif //_LL_RING_HTS_ELEM_PVT_H_
