#include <stdlib.h>
#include <limits.h>
#include <ll_common.h>
#include <ll_memzone.h>
#include <ll_errno.h>

static const struct ll_memzone *
memzone_reserve_aligned_thread_unsafe(const char *name, size_t len,
	int socket_id, unsigned int flags, unsigned int align,
	unsigned int bound) {
	
	struct ll_memzone *mz;

	//对齐大小至少为缓存对齐
	if (align < LL_CACHE_LINE_SIZE) {
		align = LL_CACHE_LINE_SIZE;
	}

	if (unlikely(len > SIZE_MAX - LL_CACHE_LINE_MASK)) {
		ll_errno = EINVAL; //长度过大
		return NULL;
	}
	
	mz = (struct ll_memzone*)malloc(sizeof(struct ll_memzone));
	if (unlikely(mz == NULL)) {
		ll_errno = ENOMEM;
		return NULL;
	}
	
	posix_memalign((void **)&mz->addr, align, len);
	if (unlikely(mz->addr == NULL)) {
		free(mz);
		return NULL;
	}
	
	mz->len = len;
	mz->hugepage_sz = ((uint64_t)-1);
	mz->flags = 0;
	
	return mz;
}

const struct ll_memzone *ll_memzone_reserve_aligned(const char *name,
			size_t len,int socket_id,unsigned flags, unsigned align) {
	return memzone_reserve_aligned_thread_unsafe(name, len, socket_id, flags, align, 0);
}

int
ll_memzone_free(const struct ll_memzone *mz) {
	free(mz->addr);
	free((void *)mz);
	return 0;
}
