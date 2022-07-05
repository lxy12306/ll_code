#include <stdio.h>
#include <string.h>

#include <ll_errno.h>
#include <ll_log.h>

#include <ll_mempool.h>


struct ll_mempool_ops_table ll_mempool_ops_table = {
	.sl = LL_SPINLOCK_INITIALIZER,
	.num_ops = 0,
};

//add a new ops struct in ll_mempool_ops_table, return its index
int
ll_mempool_register_ops(const struct ll_mempool_ops *h) {

	struct ll_mempool_ops *ops;
	int16_t ops_index;

	ll_spinlock_lock(&ll_mempool_ops_table.sl);
	
	if (ll_mempool_ops_table.num_ops >= LL_MEMPOOL_MAX_OPS_IDX) {
		ll_spinlock_unlock(&ll_mempool_ops_table.sl);
		LL_LOG(ERR, MEMPOOL, "Maximum number of mempool ops structs exceeded\n");
		return -ENOSPC;
	}

	if (h->alloc == NULL || h->enqueue == NULL ||
			h->dequeue == NULL || h->get_count == NULL) {
		ll_spinlock_unlock(&ll_mempool_ops_table.sl);
		LL_LOG(ERR, MEMPOOL,"Missing callback while registering mempool ops\n");
		return -EINVAL;
	}

	if (strlen(h->name) >= sizeof(ops->name) - 1) {
		ll_spinlock_unlock(&ll_mempool_ops_table.sl);
		LL_LOG(DEBUG, EAL, "%s(): mempool_ops <%s>: name too long\n",__func__, h->name);
		ll_errno = EEXIST;
		return -EEXIST;
	}

	ops_index = ll_mempool_ops_table.num_ops++;
	ops = &ll_mempool_ops_table.ops[ops_index];
	snprintf(ops->name, sizeof(ops->name), "%s", h->name);
	ops->alloc = h->alloc;
	ops->free = h->free;
	ops->enqueue = h->enqueue;
	ops->dequeue = h->dequeue;
	ops->get_count = h->get_count;
	ops->calc_mem_size = h->calc_mem_size;
	ops->populate = h->populate;
	ops->get_info = h->get_info;
	ops->dequeue_contig_blocks = h->dequeue_contig_blocks;

	ll_spinlock_unlock(&ll_mempool_ops_table.sl);

	return ops_index;
}

/* wrapper to allocate an external mempool's private (pool) data. */
int
ll_mempool_ops_alloc(struct ll_mempool *mp)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);
	return ops->alloc(mp);
}

/* wrapper to free an external pool ops. */
void
ll_mempool_ops_free(struct ll_mempool *mp)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);
	if (ops->free == NULL)
		return;
	ops->free(mp);
}

/* wrapper to get available objects in an external mempool. */
unsigned int
ll_mempool_ops_get_count(const struct ll_mempool *mp)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);
	return ops->get_count(mp);
}

/* wrapper to calculate the memory size required to store given number
 * of objects
 */
ssize_t
ll_mempool_ops_calc_mem_size(const struct ll_mempool *mp,
				uint32_t obj_num, uint32_t pg_shift,
				size_t *min_chunk_size, size_t *align)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);

	if (ops->calc_mem_size == NULL)
		return ll_mempool_op_calc_mem_size_default(mp, obj_num,
				pg_shift, min_chunk_size, align);

	return ops->calc_mem_size(mp, obj_num, pg_shift, min_chunk_size, align);
}

/* wrapper to populate memory pool objects using provided memory chunk */
int
ll_mempool_ops_populate(struct ll_mempool *mp, unsigned int max_objs,
				void *vaddr, ll_iova_t iova, size_t len,
				ll_mempool_populate_obj_cb_t *obj_cb,
				void *obj_cb_arg)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);

	if (ops->populate == NULL)
		return ll_mempool_op_populate_default(mp, max_objs, vaddr,
						       iova, len, obj_cb,
						       obj_cb_arg);

	return ops->populate(mp, max_objs, vaddr, iova, len, obj_cb,
			     obj_cb_arg);
}

/* wrapper to get additional mempool info */
int
ll_mempool_ops_get_info(const struct ll_mempool *mp,
			 struct ll_mempool_info *info)
{
	struct ll_mempool_ops *ops;

	ops = ll_mempool_get_ops(mp->ops_index);

	if (unlikely(ops->get_info == NULL)) {
		return -ENOTSUP;
	}
	return ops->get_info(mp, info);
}


/* sets mempool ops previously registered by ll_mempool_register_ops. */
int
ll_mempool_set_ops_byname(struct ll_mempool *mp, const char *name,
	void *pool_config)
{
	struct ll_mempool_ops *ops = NULL;
	unsigned i;

	/* too late, the mempool is already populated. */
	if (mp->flags & LL_MEMPOOL_F_POOL_CREATED)
		return -EEXIST;

	for (i = 0; i < ll_mempool_ops_table.num_ops; i++) {
		if (!strcmp(name,
				ll_mempool_ops_table.ops[i].name)) {
			ops = &ll_mempool_ops_table.ops[i];
			break;
		}
	}

	if (ops == NULL)
		return -EINVAL;

	mp->ops_index = i;
	mp->pool_config = pool_config;
	return 0;
}
