/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdio.h>
#include <string.h>

#include <ll_errno.h>
#include <ll_ring.h>
#include <ll_mempool.h>

#define LL_MEMPOOL_RING_RTS 0
#define LL_MEMPOOL_RING_HTS 0

static int
common_ring_mp_enqueue(struct ll_mempool *mp, void * const *obj_table,
		unsigned n)
{
	return ll_ring_mp_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static int
common_ring_sp_enqueue(struct ll_mempool *mp, void * const *obj_table,
		unsigned n)
{
	return ll_ring_sp_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

#if LL_MEMPOOL_RING_RTS
static int
rts_ring_mp_enqueue(struct ll_mempool *mp, void * const *obj_table,
	unsigned int n)
{
	return ll_ring_mp_rts_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}
#endif

#if LL_MEMPOOL_RING_HTS

static int
hts_ring_mp_enqueue(struct ll_mempool *mp, void * const *obj_table,
	unsigned int n)
{
	return ll_ring_mp_hts_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}
#endif

static int
common_ring_mc_dequeue(struct ll_mempool *mp, void **obj_table, unsigned n)
{
	return ll_ring_mc_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static int
common_ring_sc_dequeue(struct ll_mempool *mp, void **obj_table, unsigned n)
{
	return ll_ring_sc_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

#if LL_MEMPOOL_RING_RTS
static int
rts_ring_mc_dequeue(struct ll_mempool *mp, void **obj_table, unsigned int n)
{
	return ll_ring_mc_rts_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}
#endif

#if LL_MEMPOOL_RING_HTS
static int
hts_ring_mc_dequeue(struct ll_mempool *mp, void **obj_table, unsigned int n)
{
	return ll_ring_mc_hts_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

#endif

static unsigned
common_ring_get_count(const struct ll_mempool *mp)
{
	return ll_ring_count(mp->pool_data);
}

static int
ring_alloc(struct ll_mempool *mp, uint32_t rg_flags)
{
	int ret;
	char rg_name[LL_RING_NAMESIZE];
	struct ll_ring *r;

	ret = snprintf(rg_name, sizeof(rg_name),
		LL_MEMPOOL_MZ_FORMAT, mp->name);
	if (ret < 0 || ret >= (int)sizeof(rg_name)) {
		ll_errno = ENAMETOOLONG;
		return -ll_errno;
	}

	/*
	 * Allocate the ring that will be used to store objects.
	 * Ring functions will return appropriate errors if we are
	 * running as a secondary process etc., so no checks made
	 * in this function for that condition.
	 */
	r = ll_ring_create(rg_name, ll_align32pow2(mp->size + 1),
		mp->socket_id, rg_flags);
	if (r == NULL)
		return -ll_errno;

	mp->pool_data = r;

	return 0;
}

static int
common_ring_alloc(struct ll_mempool *mp)
{
	uint32_t rg_flags = 0;

	if (mp->flags & LL_MEMPOOL_F_SP_PUT)
		rg_flags |= RING_F_SP_ENQ;
	if (mp->flags & LL_MEMPOOL_F_SC_GET)
		rg_flags |= RING_F_SC_DEQ;

	return ring_alloc(mp, rg_flags);
}

static int
rts_ring_alloc(struct ll_mempool *mp)
{
	return ring_alloc(mp, RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ);
}

static int
hts_ring_alloc(struct ll_mempool *mp)
{
	return ring_alloc(mp, RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ);
}

static void
common_ring_free(struct ll_mempool *mp)
{
	ll_ring_free(mp->pool_data);
}

/*
 * The following 4 declarations of mempool ops structs address
 * the need for the backward compatible mempool handlers for
 * single/multi producers and single/multi consumers as dictated by the
 * flags provided to the ll_mempool_create function
 */
static const struct ll_mempool_ops ops_mp_mc = {
	.name = "ring_mp_mc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_mp_enqueue,
	.dequeue = common_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct ll_mempool_ops ops_sp_sc = {
	.name = "ring_sp_sc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_sp_enqueue,
	.dequeue = common_ring_sc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct ll_mempool_ops ops_mp_sc = {
	.name = "ring_mp_sc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_mp_enqueue,
	.dequeue = common_ring_sc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct ll_mempool_ops ops_sp_mc = {
	.name = "ring_sp_mc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_sp_enqueue,
	.dequeue = common_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};


#if LL_MEMPOOL_RING_RTS
/* ops for mempool with ring in MT_RTS sync mode */
static const struct ll_mempool_ops ops_mt_rts = {
	.name = "ring_mt_rts",
	.alloc = rts_ring_alloc,
	.free = common_ring_free,
	.enqueue = rts_ring_mp_enqueue,
	.dequeue = rts_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};
#endif

#if LL_MEMPOOL_RING_HTS
/* ops for mempool with ring in MT_HTS sync mode */
static const struct ll_mempool_ops ops_mt_hts = {
	.name = "ring_mt_hts",
	.alloc = hts_ring_alloc,
	.free = common_ring_free,
	.enqueue = hts_ring_mp_enqueue,
	.dequeue = hts_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};
#endif

LL_MEMPOOL_REGISTER_OPS(ops_mp_mc);
LL_MEMPOOL_REGISTER_OPS(ops_sp_sc);
LL_MEMPOOL_REGISTER_OPS(ops_mp_sc);
LL_MEMPOOL_REGISTER_OPS(ops_sp_mc);
#if LL_MEMPOOL_RING_RTS
LL_MEMPOOL_REGISTER_OPS(ops_mt_rts);
#endif
#if LL_MEMPOOL_RING_HTS
LL_MEMPOOL_REGISTER_OPS(ops_mt_hts);
#endif
