#include <sys/queue.h>

#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_errno.h>
#include <ll_log.h>
#include <ll_mempool.h>


#define CACHE_FLUSHTHRESH_MULTIPLIER 1.5
#define CALC_CACHE_FLUSHTHRESH(c)	\
	((typeof(c))((c) * CACHE_FLUSHTHRESH_MULTIPLIER))

static unsigned int
mem_object_align(unsigned int obj_size) {
	return obj_size;
}

static void
mempool_cache_init(struct ll_mempool_cache *cache, uint32_t size) {
	cache->size = size;
	cache->flushthresh = CALC_CACHE_FLUSHTHRESH(size);
	cache->len = 0;
}

static void
mempool_event_callback_invoke(enum ll_mempool_event event,
			      struct ll_mempool *mp)
{
	return;
}


uint32_t
ll_mempool_calc_obj_size(uint32_t elt_size, uint32_t flags,
	struct ll_mempool_objsz *sz) {
	struct ll_mempool_objsz lsz;
	
	sz = (sz != NULL) ? sz : &lsz;
	
	sz->header_size = sizeof(struct ll_mempool_objhdr);
	if ((flags & LL_MEMPOOL_F_NO_CACHE_ALIGN) == 0) {
		sz->header_size = LL_ALIGN_CEIL(sz->header_size, LL_MEMPOOL_ALIGN);
	}
#ifdef LL_LIBLL_MEMPOOL_DEBUG
	sz->trailer_size = sizeof(struct ll_mempool_objtlr);
#else
	sz->trailer_size = 0;
#endif

	//element size is 8 bytes-aligned at least
	sz->elt_size = LL_ALIGN_CEIL(elt_size, sizeof(uint64_t));

	//expand trailer to next cache line
	if ((flags & LL_MEMPOOL_F_NO_CACHE_ALIGN) == 0) {
		sz->total_size = sz->header_size + sz->elt_size +
			sz->trailer_size;
		sz->trailer_size += ((LL_MEMPOOL_ALIGN -
				 (sz->total_size & LL_MEMPOOL_ALIGN_MASK)) &
				 LL_MEMPOOL_ALIGN_MASK);
	}

	//increase trailer to add padding between objects in order to spread
	//them across memory channels/ranks
	if ((flags & LL_MEMPOOL_F_NO_SPREAD) == 0) {
		unsigned new_size;
		new_size = mem_object_align(sz->header_size + sz->elt_size + sz->trailer_size);
		sz->trailer_size = new_size - sz->header_size - sz->elt_size;
	}

	sz->total_size = sz->header_size + sz->elt_size + sz->trailer_size;

	return sz->total_size;
}

/* Get the minimal page size used in a mempool before populating it. */
int
ll_mempool_get_page_size(struct ll_mempool *mp, size_t *pg_sz) {
	bool need_iova_contig_obj;
	bool alloc_in_ext_mem = false;
	
	need_iova_contig_obj = !(mp->flags & LL_MEMPOOL_F_NO_IOVA_CONTIG);
	
	if (!need_iova_contig_obj) {
		*pg_sz = 0;
	} else if(ll_eal_has_hugepages() || alloc_in_ext_mem){
		*pg_sz = 256;
	} else  {
		*pg_sz = ll_mem_page_size();
	}

	return 0;
}

/*
 * Create and initialize a cache for objects that are retrieved from and
 * returned to an underlying mempool. This structure is identical to the
 * local_cache[lcore_id] pointed to by the mempool structure.
 */
struct ll_mempool_cache *
ll_mempool_cache_create(uint32_t size, int socket_id)
{
	struct ll_mempool_cache *cache;

	if (size == 0 || size > LL_MEMPOOL_CACHE_MAX_SIZE) {
		ll_errno = EINVAL;
		return NULL;
	}

	cache = ll_zmalloc_socket("MEMPOOL_CACHE", sizeof(*cache),
				  LL_CACHE_LINE_SIZE, socket_id);
	if (cache == NULL) {
		LL_LOG(ERR, MEMPOOL, "Cannot allocate mempool cache.\n");
		ll_errno = ENOMEM;
		return NULL;
	}

	mempool_cache_init(cache, size);

	return cache;
}


//create an empty mempool
struct ll_mempool *
ll_mempool_create_empty(const char *name, unsigned n, unsigned elt_size,
			unsigned cache_size, unsigned private_data_size,
			int socket_id, unsigned flags) {
	char mz_name[LL_MEMZONE_NAMESIZE];
	struct ll_mempool *mp = NULL;
	struct ll_mempool_objsz objsz = {0};
	//struct ll_tailq_entry *te = NULL;
	const struct ll_memzone *mz = NULL;
	size_t mempool_size;
	unsigned int mz_flags = 0;

	unsigned lcore_id;

	int ret;
	
	LL_BUILD_BUG_ON((sizeof(struct ll_mempool) &
				LL_CACHE_LINE_MASK) != 0);
	LL_BUILD_BUG_ON((sizeof(struct ll_mempool_cache) &
				LL_CACHE_LINE_MASK) != 0);
#ifdef LL_LIBLL_MEMPOOL_DEBUG
	LL_BUILD_BUG_ON((sizeof(struct ll_mempool_debug_stats) &
				LL_CACHE_LINE_MASK) != 0);
	LL_BUILD_BUG_ON((offsetof(struct ll_mempool, stats) &
				LL_CACHE_LINE_MASK) != 0);
#endif

	//if asked for zero items, just return
	if (n == 0) {
		ll_errno = EINVAL;
		return NULL;
	}
	
	//if asked cache too big
	if (cache_size > LL_MEMPOOL_CACHE_MAX_SIZE ||
		CALC_CACHE_FLUSHTHRESH(cache_size) > n) {
		ll_errno = EINVAL;
		LL_LOG(DEBUG,MEMPOOL, "mempool cache is to big\n");	
		return NULL;
	}
	
	//enforce only user flags are passed by the application
	if ((flags & ~LL_MEMPOOL_VALID_USER_FLAGS)) {
		ll_errno = EINVAL;
		LL_LOG(DEBUG,MEMPOOL, "input invalid flags\n");
		return NULL;
	}
	
	//in the begging, there are no elems, so we need to set the flags
	flags |= LL_MEMPOOL_F_NON_IO;
	
	// no cache align imply no spread
	if (flags & LL_MEMPOOL_F_NO_CACHE_ALIGN) {
		flags |= LL_MEMPOOL_F_NO_SPREAD;
	}
	
	//calculate mempool object sizes
	if (!ll_mempool_calc_obj_size(elt_size, flags, &objsz)) {
		ll_errno = EINVAL;
		LL_LOG(DEBUG, MEMPOOL,"invalid elt size\n");
		return NULL;
	}
	
	private_data_size = (private_data_size + LL_MEMPOOL_ALIGN_MASK) & (~LL_MEMPOOL_ALIGN_MASK);

	mempool_size = LL_MEMPOOL_HEADER_SIZE(mp, cache_size);
	mempool_size += private_data_size;
	mempool_size = LL_ALIGN_CEIL(mempool_size, LL_MEMPOOL_ALIGN);
	
	ret = snprintf(mz_name, sizeof(mz_name), LL_MEMPOOL_MZ_PREFIX"%s", name);
	if (ret < 0 || ret >= (int)sizeof(mz_name)) {
		ll_errno = ENAMETOOLONG;
		goto __exit;
	}
	
	mz = ll_memzone_reserve(mz_name, mempool_size, socket_id, mz_flags);
	if (mz == NULL) {
		LL_LOG(DEBUG, MEMPOOL, "reserve mz failed\n");
		goto __exit;
	}

	//init mp structure
	mp = mz->addr;
	memset(mp, 0, LL_MEMPOOL_HEADER_SIZE(mp, cache_size));
	ret = snprintf(mp->name, sizeof(mp->name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(mp->name)) {
		ll_errno = ENAMETOOLONG;
		goto __exit;
	}
	mp->mz = mz;
	mp->size = n;
	mp->flags = flags;
	mp->socket_id = socket_id;
	mp->elt_size = objsz.elt_size;
	mp->header_size = objsz.header_size;
	mp->trailer_size = objsz.trailer_size;
	
	mp->cache_size = cache_size;
	mp->private_data_size = private_data_size;
	STAILQ_INIT(&mp->elt_list);
	STAILQ_INIT(&mp->mem_list);
	
	//init the cache
	mp->local_cache = (struct ll_mempool_cache *)
		LL_PTR_ADD(mp, LL_MEMPOOL_HEADER_SIZE(mp, 0));
	/* Init all default caches. */
	if (cache_size != 0) {
		for (lcore_id = 0; lcore_id < LL_MAX_LCORE; lcore_id++)
			mempool_cache_init(&mp->local_cache[lcore_id],
					   cache_size);
	}
	return mp;
	
__exit:
	ll_mempool_free(mp);
	return NULL;	
}

static int
mempool_ops_alloc_once(struct ll_mempool *mp) {
	int ret;
	
	/*create the internal ring if not already done*/
	if ((mp->flags & LL_MEMPOOL_F_POOL_CREATED) == 0) {
		ret = ll_mempool_ops_alloc(mp);
		if (ret != 0) {
			return ret;
		}
		mp->flags |= LL_MEMPOOL_F_POOL_CREATED;
	}
	return 0;
}

static void
mempool_add_elem(struct ll_mempool *mp, __ll_unused void *opaque,
		 void *obj, ll_iova_t iova) {
	struct ll_mempool_objhdr *hdr;
	struct ll_mempool_objtlr *tlr __ll_unused;

	/* set mempool ptr in header */
	hdr = LL_PTR_SUB(obj, sizeof(*hdr));
	hdr->mp = mp;
	hdr->iova = iova;
	STAILQ_INSERT_TAIL(&mp->elt_list, hdr, next);
	mp->populated_size++;

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	hdr->cookie = LL_MEMPOOL_HEADER_COOKIE2;
	tlr = ll_mempool_get_trailer(obj);
	tlr->cookie = LL_MEMPOOL_TRAILER_COOKIE;
#endif
}

/* Free memory chunks used by a mempool. Objects must be in pool */
static void
ll_mempool_free_memchunks(struct ll_mempool *mp)
{
	struct ll_mempool_memhdr *memhdr;
	void *elt;

	while (!STAILQ_EMPTY(&mp->elt_list)) {
		ll_mempool_ops_dequeue_bulk(mp, &elt, 1);
		LL_SET_USED(elt);
		STAILQ_REMOVE_HEAD(&mp->elt_list, next);
		mp->populated_size--;
	}

	while (!STAILQ_EMPTY(&mp->mem_list)) {
		memhdr = STAILQ_FIRST(&mp->mem_list);
		STAILQ_REMOVE_HEAD(&mp->mem_list, next);
		if (memhdr->free_cb != NULL)
			memhdr->free_cb(memhdr, memhdr->opaque);
		ll_free(memhdr);
		mp->nb_mem_chunks--;
	}
}


/* Add objects in the pool, using a physically contiguous memory
 * zone. Return the number of objects added, or a negative value
 * on error.
 */
int
ll_mempool_populate_iova(struct ll_mempool *mp, char *vaddr,
	ll_iova_t iova, size_t len, ll_mempool_memchunk_free_cb_t *free_cb,
	void *opaque) {

	unsigned i = 0;
	size_t off;
	struct ll_mempool_memhdr *memhdr;
	int ret;
	
	ret = mempool_ops_alloc_once(mp);
	if (ret != 0)
		return ret;

	
	/* mempool is already populated */
	if (mp->populated_size >= mp->size)
		return -ENOSPC;

	memhdr = ll_zmalloc("MEMPOOL_MEMHDR", sizeof(*memhdr), 0);
	if (memhdr == NULL)
		return -ENOMEM;

	memhdr->mp = mp;
	memhdr->addr = vaddr;
	memhdr->iova = iova;
	memhdr->len = len;
	memhdr->free_cb = free_cb;
	memhdr->opaque = opaque;

	if (mp->flags & LL_MEMPOOL_F_NO_CACHE_ALIGN) {
		off = LL_PTR_ALIGN_CEIL(vaddr, 8) - vaddr;
	} else {
		off = LL_PTR_ALIGN_CEIL(vaddr, LL_MEMPOOL_ALIGN) - vaddr;
	}
	
	if (off > len) {
		ret = 0;
		goto fail;
	}
	
	i = ll_mempool_ops_populate(mp, mp->size - mp->populated_size,
		(char *)vaddr + off,
		(iova == LL_BAD_IOVA) ? LL_BAD_IOVA : (iova + off),
		len - off, mempool_add_elem, NULL);

	/* not enough room to store one object */
	if (i == 0) {
		ret = 0;
		goto fail;
	}

	
	STAILQ_INSERT_TAIL(&mp->mem_list, memhdr, next);
	mp->nb_mem_chunks++;
	
	/* Check if at least some objects in the pool are now usable for IO. */
	if (!(mp->flags & LL_MEMPOOL_F_NO_IOVA_CONTIG) && iova != LL_BAD_IOVA)
		mp->flags &= ~LL_MEMPOOL_F_NON_IO;

	/* Report the mempool as ready only when fully populated. */
	if (mp->populated_size >= mp->size)
		mempool_event_callback_invoke(LL_MEMPOOL_EVENT_READY, mp);

	return i;

fail:
	ll_free(memhdr);
	return ret;
}


static ll_iova_t
get_iova(void *addr)
{
	struct ll_memseg *ms;

	/* try registered memory first */
	ms = ll_mem_virt2memseg(addr, NULL);
	if (ms == NULL || ms->iova == LL_BAD_IOVA)
		/* fall back to actual physical address */
		return ll_mem_virt2iova(addr);
	return ms->iova + LL_PTR_DIFF(addr, ms->addr);
}

/* Populate the mempool with a virtual area. Return the number of
 * objects added, or a negative value on error.
 */
int
ll_mempool_populate_virt(struct ll_mempool *mp, char *addr,
	size_t len, size_t pg_sz, ll_mempool_memchunk_free_cb_t *free_cb,
	void *opaque)
{
	ll_iova_t iova;
	size_t off, phys_len;
	int ret, cnt = 0;

	if (mp->flags & LL_MEMPOOL_F_NO_IOVA_CONTIG)
		return ll_mempool_populate_iova(mp, addr, LL_BAD_IOVA,
			len, free_cb, opaque);

	for (off = 0; off < len && mp->populated_size < mp->size; off += phys_len) {
		iova = get_iova(addr + off);

		for (phys_len = LL_MIN((size_t)(LL_PTR_ALIGN_CEIL(addr + off + 1, pg_sz) - addr - off), len - off);
			off + phys_len < len;
			phys_len = LL_MIN(phys_len + pg_sz, len - off)) {
			ll_iova_t iova_tmp;

			iova_tmp = get_iova(addr + off + phys_len); //get va next page's iova

			if (iova_tmp == LL_BAD_IOVA ||
					iova_tmp != iova + phys_len) 
				break; //if iova is not contigous, break;
		} //find iova contigous memory size( begin with addr + off)
		
		ret = ll_mempool_populate_iova(mp, addr + off, iova, phys_len, free_cb, opaque);
		if (ret == 0)
			continue;
		if (ret < 0)
			goto fail;

		/* no need to call the free callback for next chunks */
		free_cb = NULL;
		cnt += ret;
	}
	return cnt;
fail:
	return ret;
}


/* free a memchunk allocated with ll_memzone_reserve() */
static void
ll_mempool_memchunk_mz_free(__ll_unused struct ll_mempool_memhdr *memhdr,
	void *opaque)
{
	const struct ll_memzone *mz = opaque;
	ll_memzone_free(mz);
}


/* Default function to populate the mempool: allocate memory in memzones,
 * and populate them. Return the number of objects added, or a negative
 * value on error.
 */
int
ll_mempool_populate_default(struct ll_mempool *mp) {
	unsigned int mz_flags = LL_MEMZONE_1GB | LL_MEMZONE_SIZE_HINT_ONLY;
	const struct ll_memzone *mz;
	char mz_name[LL_MEMZONE_NAMESIZE];
	bool need_iova_contig_obj;
	size_t align, pg_sz, pg_shift = 0;
	unsigned mz_id, n;
	int mem_size;
	size_t max_alloc_size;
	ll_iova_t iova;

	int ret;
	
	ret = mempool_ops_alloc_once(mp);
	if (ret != 0) {
		return ret;
	}

	/* mempool must not be populated */
	if (mp->nb_mem_chunks != 0)
		return -EEXIST;
	
	need_iova_contig_obj = !(mp->flags & LL_MEMPOOL_F_NO_IOVA_CONTIG);
	ret = ll_mempool_get_page_size(mp,&pg_sz);
	if (ret < 0) {
		return ret;
	}

	if (pg_sz != 0) {
		pg_shift = ll_bsf32(pg_sz);
	}
	
	for (mz_id = 0, n = mp->size; n > 0; mz_id++, n -= ret) {
		size_t min_chunk_size;
		max_alloc_size = SIZE_MAX;
		
		mem_size = ll_mempool_ops_calc_mem_size(
			mp, n, pg_shift, &min_chunk_size, &align);

		if (mem_size < 0) {
			ret = mem_size;
			goto fail;
		}

		ret = snprintf(mz_name, sizeof(mz_name),
			LL_MEMPOOL_MZ_FORMAT "_%d", mp->name, mz_id);
		if (ret < 0 || ret >= (int)sizeof(mz_name)) {
			ret = -ENAMETOOLONG;
			goto fail;
		}
		
		/* if we're trying to reserve contiguous memory, add appropriate
		 * memzone flag.
		 */
		if (min_chunk_size == mem_size) {
			mz_flags |= LL_MEMZONE_IOVA_CONTIG;
		}
		
		do {
			mz = ll_memzone_reserve_aligned(mz_name, LL_MIN(max_alloc_size, (size_t)mem_size), mp->socket_id, mz_flags, align);
			if (mz != NULL || ll_errno != ENOMEM)
				break;

			max_alloc_size = LL_MIN(max_alloc_size, (size_t)mem_size) / 2; // try to alloc memory，if can not， try to alloc halved memory
		} while(mz == NULL && max_alloc_size >= min_chunk_size);

		if (mz == NULL) {
			ret = -ll_errno;
			goto fail;
		}
		
		if (need_iova_contig_obj) {
			iova = mz->iova;
		} else {
			iova = LL_BAD_IOVA;
		}

		if (pg_sz == 0 || (mz->flags & LL_MEMZONE_IOVA_CONTIG))
			ret = ll_mempool_populate_iova(mp, mz->addr,
				iova, mz->len,
				ll_mempool_memchunk_mz_free,
				(void *)(uintptr_t)mz);
		else
			ret = ll_mempool_populate_virt(mp, mz->addr,
				mz->len, pg_sz,
				ll_mempool_memchunk_mz_free,
				(void *)(uintptr_t)mz);
		if (ret == 0) /* should not happen */
			ret = -ENOBUFS;
		if (ret < 0) {
			ll_memzone_free(mz);
			goto fail;
		}
	}
	
	return mp->size;
fail:
	ll_mempool_free_memchunks(mp);
	return 0;
}

/**
 * Call a function for each mempool element
 *
 * Iterate across all objects attached to a ll_mempool and call the
 * callback function on it.
 */
uint32_t ll_mempool_obj_iter(struct ll_mempool *mp,
	ll_mempool_obj_cb_t *obj_cb, void *obj_cb_arg) {

	struct ll_mempool_objhdr *hdr;
	void *obj;
	unsigned n = 0;
	
	STAILQ_FOREACH(hdr, &mp->elt_list, next) {
		obj = (char *)hdr + sizeof(*hdr);
		obj_cb(mp, obj_cb_arg, obj, n);
		n++;
	}

	return n;
}

/* create the mempool */
struct ll_mempool *
ll_mempool_create(const char *name, unsigned n, unsigned elt_size,
	unsigned cache_size, unsigned private_data_size,
	ll_mempool_ctor_t *mp_init, void *mp_init_arg,
	ll_mempool_obj_cb_t *obj_init, void *obj_init_arg,
	int socket_id, unsigned flags) {

	int ret;
	struct ll_mempool *mp;
	
	mp = ll_mempool_create_empty(name, n, elt_size, cache_size, private_data_size, socket_id, flags);
	if (mp == NULL) {
		return NULL;
	}

	/**
	 * Since we have 4 combinations of the SP/SC/MP/MC examin the flags to 
	 * set the correct index into the table of ops structs
	 */
	if ((flags & LL_MEMPOOL_F_SP_PUT) && (flags & LL_MEMPOOL_F_SC_GET))
		ret = ll_mempool_set_ops_byname(mp, "ring_sp_sc", NULL);
	else if (flags & LL_MEMPOOL_F_SP_PUT)
		ret = ll_mempool_set_ops_byname(mp, "ring_sp_mc", NULL);
	else if (flags & LL_MEMPOOL_F_SC_GET)
		ret = ll_mempool_set_ops_byname(mp, "ring_mp_sc", NULL);
	else
		ret = ll_mempool_set_ops_byname(mp, "ring_mp_mc", NULL);

	if (ret)
		goto fail;

	if (mp_init)
		mp_init(mp, mp_init_arg);

	if (ll_mempool_populate_default(mp) < 0)
		goto fail;

	/* call the object initializers */
	if (obj_init)
		ll_mempool_obj_iter(mp, obj_init, obj_init_arg);

	return mp;

fail:
	ll_mempool_free(mp);
	return NULL;
}

/* free a mempool */
void
ll_mempool_free(struct ll_mempool *mp)
{

	mempool_event_callback_invoke(LL_MEMPOOL_EVENT_DESTROY, mp);
	ll_mempool_free_memchunks(mp);
	ll_mempool_ops_free(mp);
	ll_memzone_free(mp->mz);
}
