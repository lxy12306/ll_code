#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#include <ll_common.h>
#include <ll_errno.h>
#include <ll_mem_config.h>
#include <ll_log.h>
#include <ll_ring.h>

#include "ll_ring_rts.h"


/* mask of all valid flag values to ring_create() */
#define RING_F_MASK (RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ | \
		     RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ |	       \
		     RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ)

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

/* by default set head/tail distance as 1/8 of ring capacity */
#define HTD_MAX_DEF	8

/* return the size of memory occupied by a ring */
ssize_t
ll_ring_get_memsize_elem(unsigned int esize, unsigned int count)
{
	ssize_t sz;

	/* Check if element size is a multiple of 4B */
	if ((esize & 3) != 0) {
		LL_LOG(ERR, RING, "element size is not a multiple of 4\n");

		return -EINVAL;
	}

	/* count must be a power of 2 */
	if ((!POWEROF2(count)) || (count > LL_RING_SZ_MASK)) {
		LL_LOG(ERR, RING,
			"Requested number of elements is invalid, must be power of 2, and not exceed %u\n",
			LL_RING_SZ_MASK);

		return -EINVAL;
	}

	sz = sizeof(struct ll_ring) + count * esize;
	sz = LL_ALIGN(sz, LL_CACHE_LINE_SIZE);
	return sz;
}

/*return the size of memory occupied by a ring*/ 
ssize_t
ll_ring_get_memsize(unsigned int count) {
	return ll_ring_get_memsize_elem(sizeof(void *), count);
}

//internal helper function to reset prod/cons head-tail values
static void reset_headtail(void *p) {
	struct ll_ring_headtail *ht;
	struct ll_ring_rts_headtail *ht_rts;
	struct ll_ring_hts_headtail *ht_hts;
	
	ht =(struct ll_ring_headtail*) p;
	ht_hts = (struct ll_ring_hts_headtail*)p;
	ht_rts = (struct ll_ring_rts_headtail*)p;
	
	switch(ht->sync_type) {
		case LL_RING_SYNC_MT:
		case LL_RING_SYNC_ST:
			ht->head = 0;
			ht->tail = 0;
			break;
		case LL_RING_SYNC_MT_RTS:
			ht_rts->head.raw = 0;
			ht_rts->tail.raw = 0;
			break;
		case LL_RING_SYNC_MT_HTS:
			ht_hts->ht.raw = 0;
			break;
		default:
			break;
	}
}

void
ll_ring_reset(struct ll_ring *r) {
	reset_headtail(&r->prod);
	reset_headtail(&r->cons);
}

static int
get_sync_type(uint32_t flags, enum ll_ring_sync_type *prod_st,
	enum ll_ring_sync_type *cons_st)	
{
	static const uint32_t prod_st_flags =
		(RING_F_SP_ENQ | RING_F_MP_RTS_ENQ | RING_F_MP_HTS_ENQ);
	static const uint32_t cons_st_flags =
		(RING_F_SC_DEQ | RING_F_MC_RTS_DEQ | RING_F_MC_HTS_DEQ);

	switch (flags & prod_st_flags) {
	case 0:
		*prod_st = LL_RING_SYNC_MT;
		break;
	case RING_F_SP_ENQ:
		*prod_st = LL_RING_SYNC_ST;
		break;
	case RING_F_MP_RTS_ENQ:
		*prod_st = LL_RING_SYNC_MT_RTS;
		break;
	case RING_F_MP_HTS_ENQ:
		*prod_st = LL_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	switch (flags & cons_st_flags) {
	case 0:
		*cons_st = LL_RING_SYNC_MT;
		break;
	case RING_F_SC_DEQ:
		*cons_st = LL_RING_SYNC_ST;
		break;
	case RING_F_MC_RTS_DEQ:
		*cons_st = LL_RING_SYNC_MT_RTS;
		break;
	case RING_F_MC_HTS_DEQ:
		*cons_st = LL_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int
ll_ring_init(struct ll_ring *r, const char *name, unsigned int count,
	unsigned int flags)
{
	int ret;

	/* compilation-time checks */
	LL_BUILD_BUG_ON((sizeof(struct ll_ring) &
			  LL_CACHE_LINE_MASK) != 0);
	LL_BUILD_BUG_ON((offsetof(struct ll_ring, cons) &
			  LL_CACHE_LINE_MASK) != 0);
	LL_BUILD_BUG_ON((offsetof(struct ll_ring, prod) &
			  LL_CACHE_LINE_MASK) != 0);

	LL_BUILD_BUG_ON(offsetof(struct ll_ring_headtail, sync_type) !=
		offsetof(struct ll_ring_hts_headtail, sync_type));
	LL_BUILD_BUG_ON(offsetof(struct ll_ring_headtail, tail) !=
		offsetof(struct ll_ring_hts_headtail, ht.pos.tail));

	LL_BUILD_BUG_ON(offsetof(struct ll_ring_headtail, sync_type) !=
		offsetof(struct ll_ring_rts_headtail, sync_type));
	LL_BUILD_BUG_ON(offsetof(struct ll_ring_headtail, tail) !=
		offsetof(struct ll_ring_rts_headtail, tail.val.pos));

	/* future proof flags, only allow suppolld values */
	if (flags & ~RING_F_MASK) {
		LL_LOG(ERR, RING,
			"Unsuppolld flags requested %#x\n", flags);
		return -EINVAL;
	}

	/* init the ring structure */
	memset(r, 0, sizeof(struct ll_ring));
	ret = (size_t)snprintf(r->name, sizeof(r->name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(r->name))
		return -ENAMETOOLONG;
	r->flags = flags;
	ret = get_sync_type(flags, &r->prod.sync_type, &r->cons.sync_type);
	if (ret != 0)
		return ret;

	if (flags & RING_F_EXACT_SZ) {
		r->size = ll_align32pow2(count + 1);
		r->mask = r->size - 1;
		r->capacity = count;
	} else {
		if ((!POWEROF2(count)) || (count > LL_RING_SZ_MASK)) {
			LL_LOG(ERR, RING,
				"Requested size is invalid, must be power of 2, and not exceed the size limit %u\n",
				LL_RING_SZ_MASK);
			return -EINVAL;
		}
		r->size = count;
		r->mask = count - 1;
		r->capacity = r->mask;
	}

	/* set default values for head-tail distance */
	if (flags & RING_F_MP_RTS_ENQ)
		ll_ring_set_prod_htd_max(r, r->capacity / HTD_MAX_DEF);
	if (flags & RING_F_MC_RTS_DEQ)
		ll_ring_set_cons_htd_max(r, r->capacity / HTD_MAX_DEF);

	return 0;
}


struct ll_ring*
ll_ring_create_elem(const char* name, unsigned int esize, unsigned int count, int socket_id, unsigned int flags) {
	
	char mz_name[LL_MEMZONE_NAMESIZE];
	struct ll_ring *r;
	const struct ll_memzone *mz;
	ssize_t ring_size;
	int mz_flags = 0;
	const unsigned int requested_count = count;
	int ret;

	if(likely(flags & RING_F_EXACT_SZ)) {
		count = ll_align32pow2(count + 1);
		//使得count向2次幂对齐
	}
	
	ring_size = ll_ring_get_memsize_elem(esize, count);
	if(unlikely(ring_size < 0)) {
		ll_errno = ring_size;
		return NULL;
	}
	
	ret = snprintf(mz_name, sizeof(mz_name), "%s%s",
		LL_RING_MZ_PREFIX, name);
	if(unlikely(ret < 0 || ret >= (int)sizeof(mz_name))) {
		ll_errno = ENAMETOOLONG;
		return NULL;
	}

	mz = ll_memzone_reserve_aligned(mz_name, ring_size, socket_id,
		mz_flags, __alignof(*r));
	
	if(likely(mz != NULL)) {
		r = (struct ll_ring*)mz->addr;
		
		ll_ring_init(r, name, requested_count, flags);
		//init ring

		r->memzone = mz;
	} else {
		r = NULL;
		LL_LOG(ERR, RING, "Cannot reserve memory\n");
	}

	return r;
}

struct ll_ring *
ll_ring_create(const char* name, unsigned int count, int socket_id, unsigned int flags) {
	return ll_ring_create_elem(name, sizeof(void *), count, socket_id, flags);
}


void
ll_ring_free(struct ll_ring *r) {
	if (unlikely(r == NULL)) {
		return;
	}
	
	if (unlikely(r->memzone == NULL)) {
		LL_LOG(ERR, RING, "Cannot free ring, not create with ll_ring_create()\n");
		return;
	}

	if (unlikely(ll_memzone_free(r->memzone) != 0)) {
		LL_LOG(ERR, RING, "Cannot free memory\n");
		return;
	}
	r = NULL;
	return;
}

void
ll_ring_dump(FILE *f, const struct ll_ring *r) {
	fprintf(f, "ring <%s>@%p\n", r->name, r);
	fprintf(f, " flags=%x\n", r->flags);
	fprintf(f, " size= %"PRIu32"\n", r->size);
	fprintf(f, " capacity= %"PRIu32"\n", r->capacity);
	fprintf(f, " ct= %"PRIu32"\n", r->cons.tail);
	fprintf(f, " ch= %"PRIu32"\n", r->cons.head);
	fprintf(f, " pt= %"PRIu32"\n", r->prod.tail);
	fprintf(f, " ph= %"PRIu32"\n", r->prod.head);
	fprintf(f, " used= %u\n", ll_ring_count(r));
	fprintf(f, " size= %"PRIu32"\n", ll_ring_free_count(r));
}

