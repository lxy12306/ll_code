
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <ll_common.h>
#include <ll_log.h>
#include <ll_errno.h>
#include <ll_fbarray.h>

#include "../mem/eal_memory.h"
#include "../private/eal_private.h"

#define MASK_SHIFT 6ULL
#define MASK_ALIGN (1ULL << MASK_SHIFT)
#define MASK_LEN_TO_IDX(x) ((x) >> MASK_SHIFT)
#define MASK_LEN_TO_MOD(x) ((x) - LL_ALIGN_FLOOR(x, MASK_ALIGN))
#define MASK_GET_IDX(idx, mod) ((idx << MASK_SHIFT) + mod)

struct used_mask {
	unsigned int n_masks;
	unsigned int fd;
	uint64_t data[];
};

#define FBARRAY_NAME_FMT "%s/fbarray_%s"
static inline const char *
get_fbarray_path(char *buffer, size_t buflen, const char *name) {
	snprintf(buffer, buflen, FBARRAY_NAME_FMT, ".",
			name);
	return buffer;
}


static size_t
calc_mask_size(unsigned int len)
{
	/* mask must be multiple of MASK_ALIGN, even though length of array
	 * itself may not be aligned on that boundary.
	 */
	len = LL_ALIGN_CEIL(len, MASK_ALIGN);
	return sizeof(struct used_mask) +
			sizeof(uint64_t) * MASK_LEN_TO_IDX(len);
}

static size_t
calc_data_size(size_t page_sz, unsigned int elt_sz, unsigned int len)
{
	size_t data_sz = elt_sz * len;
	size_t msk_sz = calc_mask_size(len);
	return LL_ALIGN_CEIL(data_sz + msk_sz, page_sz);
}

static struct used_mask *
get_used_mask(void *data, unsigned int elt_sz, unsigned int len)
{
	return (struct used_mask *) LL_PTR_ADD(data, elt_sz * len);
}

static int
resize_and_map(int fd, const char *path, void *addr, size_t len)
{
	void *map_addr;
	int  flags = MAP_FIXED | MAP_SHARED;
	int  port = PROT_READ | PROT_WRITE;

	if (ftruncate(fd, len)) { //设置文件大小
		LL_LOG(ERR, EAL, "Cannot truncate %s\n", path);
		return -1;
	}
	
	map_addr = mmap(addr, len, port, flags, fd, 0);
	if (map_addr != addr) {
		return -1;
	}

	return 0;
}

int
ll_fbarray_init(struct ll_fbarray *arr, const char *name, unsigned int len, unsigned int elt_sz) {
	char path[PATH_MAX];
	size_t page_sz, mmap_len;
	int fd = -1;
	void *data = NULL;
	struct used_mask *msk;


	if (arr == NULL) {
		ll_errno = EINVAL;
		return -1;
	}
	
	page_sz = sysconf(_SC_PAGESIZE);
	if (page_sz == (size_t)-1) {
		ll_errno = EINTR;
		return -1;
	}

	mmap_len = calc_data_size(page_sz, elt_sz, len);
	
	data = eal_get_virtual_area(NULL, &mmap_len, page_sz, 0, 0);
	if (data == NULL) {
		return -1;
	}

	get_fbarray_path(path, sizeof(path), name);

	fd = open(path, O_RDWR | O_CREAT, 0600);
	if (fd == -1) {
		ll_errno = errno;
		LL_LOG(DEBUG, EAL, "%s(): couldn't open %s: %s\n",
				__func__, path, ll_strerror(ll_errno));
		goto fail;
	} else if (eal_file_lock(fd, EAL_FLOCK_EXCLUSIVE, EAL_FLOCK_RETURN)) {
		LL_LOG(DEBUG, EAL, "%s(): couldn't lock %s: %s\n",
				__func__, path, ll_strerror(ll_errno));
		ll_errno = EBUSY;
		goto fail;
	}

	/* take out a non-exclusive lock, so that other processes could
	 * still attach to it, but no other process could reinitialize
	 * it.
	 */
	if (eal_file_lock(fd, EAL_FLOCK_SHARED, EAL_FLOCK_RETURN))
		goto fail;

	if (resize_and_map(fd, path, data, mmap_len))
		goto fail;
	
	/* initialize the data */
	memset(data, 0, mmap_len);

	/* populate data structure */
	snprintf(arr->name, sizeof(arr->name), "%s", name);
	arr->data = data;
	arr->len = len;
	arr->elt_sz = elt_sz;
	arr->count = 0;

	msk = get_used_mask(data, elt_sz, len);
	msk->n_masks = MASK_LEN_TO_IDX(LL_ALIGN_CEIL(len, MASK_ALIGN));
	msk->fd = fd;

	ll_rwlock_init(&arr->rwlock);
	return 0;

fail:
	if (data)
		eal_mem_unmap(data, mmap_len);
	if (fd >= 0)
		close(fd);
	return -1;
}

int
ll_fbarray_destroy(struct ll_fbarray *arr) {
	size_t mmap_len;
	int fd, ret;
	struct used_mask *msk;
	char path[PATH_MAX];

	if (arr == NULL) {
		ll_errno = EINVAL;
		return -1;
	}

	size_t page_sz = sysconf(_SC_PAGESIZE);
	if (page_sz == (size_t)-1) {
		ll_errno = EINTR;
		return -1;
	}

	mmap_len = calc_data_size(page_sz, arr->elt_sz, arr->len);

	msk = get_used_mask(arr->data, arr->elt_sz, arr->len);
	fd = msk->fd;
	if (eal_file_lock(fd, EAL_FLOCK_EXCLUSIVE, EAL_FLOCK_RETURN)) {
		LL_LOG(DEBUG, EAL, "Cannot destroy fbarray - another process is using it\n");
		ll_errno = EBUSY;
		ret = -1;
		goto out;
	}
	
	get_fbarray_path(path, sizeof(path), arr->name);
	if (unlink(path)) {
			LL_LOG(DEBUG, EAL, "Cannot unlink fbarray: %s\n",
				strerror(errno));
			ll_errno = errno;
			/*
			 * we're still holding an exclusive lock, so drop it to
			 * shared.
			 */
			eal_file_lock(fd, EAL_FLOCK_SHARED, EAL_FLOCK_RETURN);

			ret = -1;
			goto out;
	}

	close(fd);
	eal_mem_unmap(arr->data, mmap_len);
	ret = 0;

out:
	return ret; 
}

void *
ll_fbarray_get(const struct ll_fbarray *arr, unsigned int idx)
{
	void *ret = NULL;
	if (arr == NULL) {
		ll_errno = EINVAL;
		return NULL;
	}

	if (idx >= arr->len) {
		ll_errno = EINVAL;
		return NULL;
	}

	ret = LL_PTR_ADD(arr->data, idx * arr->elt_sz);

	return ret;
}


static int
find_next(const struct ll_fbarray *arr, unsigned int start, bool used) {
	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz, arr->len);
	
	unsigned int idx, first, first_mod;
	unsigned int last, last_mod;
	uint64_t last_msk, ignore_msk;

	/*
	 * mask only has granularity of MASK_ALIGN, but start may not be aligned
	 * on that boundary, so construct a special mask to exclude anything we
	 * don't want to see to avoid confusing ctz.
	 */
	first_mod = MASK_LEN_TO_MOD(start);
	first = MASK_LEN_TO_IDX(start);
	ignore_msk = ~((1ULL << first_mod) - 1ULL);

	/* array length may not be aligned, so calculate ignore mask for last
	 * mask index.
	 */
	last = MASK_LEN_TO_IDX(arr->len);
	last_mod = MASK_LEN_TO_MOD(arr->len);
	last_msk = ~(UINT64_MAX << last_mod);

	for (idx = first; idx < msk->n_masks; idx++) {
		uint64_t cur = msk->data[idx];
		int found;
		
		if (!used)
			cur = ~cur;

		if (idx == last) 
			cur &= last_msk;

		/* ignore everything before start on first iteration */
		if (idx == first)
			cur &= ignore_msk;
		
		if (cur == 0)
			continue;
		/*
		 * find first set bit - that will correspond to whatever it is
		 * that we're looking for.
		 */
		found = __builtin_ctzll(cur);
		return MASK_GET_IDX(idx, found);
	}
	/* we didn't find anything */
	ll_errno = used ? ENOENT : ENOSPC;
	return -1;
}

static int
find_next_n(const struct ll_fbarray *arr, unsigned int start, unsigned int n, bool used) {

	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz, arr->len);
	unsigned int msk_idx,lookahead_idx, first, first_mod;
	unsigned int last, last_mod;
	uint64_t last_msk, ignore_msk;

	/*
	 * mask only has granularity of MASK_ALIGN, but start may not be aligned
	 * on that boundary, so construct a special mask to exclude anything we
	 * don't want to see to avoid confusing ctz.
	 */
	first_mod = MASK_LEN_TO_MOD(start);
	first = MASK_LEN_TO_IDX(start);
	ignore_msk = ~((1ULL << first_mod) - 1ULL);

	/* array length may not be aligned, so calculate ignore mask for last
	 * mask index.
	 */
	last = MASK_LEN_TO_IDX(arr->len);
	last_mod = MASK_LEN_TO_MOD(arr->len);
	last_msk = ~(UINT64_MAX << last_mod);

	for (msk_idx = first; msk_idx < msk->n_masks;) {
		uint64_t cur_msk, lookahead_msk;
		unsigned int run_start, clz, left;
		bool found = false;
		/*
		 * The process of getting n consecutive bits for arbitrary n is
		 * a bit involved, but here it is in a nutshell:
		 *
		 *  1. let n be the number of consecutive bits we're looking for
		 *  2. check if n can fit in one mask, and if so, do n-1
		 *     rshift-ands to see if there is an appropriate run inside
		 *     our current mask
		 *    2a. if we found a run, bail out early
		 *    2b. if we didn't find a run, proceed
		 *  3. invert the mask and count leading zeroes (that is, count
		 *     how many consecutive set bits we had starting from the
		 *     end of current mask) as k
		 *    3a. if k is 0, continue to next mask
		 *    3b. if k is not 0, we have a potential run
		 *  4. to satisfy our requirements, next mask must have n-k
		 *     consecutive set bits right at the start, so we will do
		 *     (n-k-1) rshift-ands and check if first bit is set.
		 *
		 * Step 4 will need to be repeated if (n-k) > MASK_ALIGN until
		 * we either run out of masks, lose the run, or find what we
		 * were looking for.
		 */

		cur_msk = msk->data[msk_idx];
		left = n;

		if (!used)
			cur_msk = ~cur_msk;

		if (msk_idx == last) 
			ignore_msk &= last_msk;

		/* ignore everything before start on first iteration */
		if (ignore_msk) {
			cur_msk &= ignore_msk;
			ignore_msk = 0;
		}
		/* if n can fit in within a single mask, do a search */
		if (n <= MASK_ALIGN) {
			uint64_t tmp_msk = cur_msk;
			unsigned int s_idx;
			for (s_idx = 0; s_idx < n - 1; s_idx++)
				tmp_msk &= tmp_msk << 1ULL;
			/* we found what we were looking for */
			if (tmp_msk != 0) {
				run_start = __builtin_ctzll(tmp_msk) - n + 1;
				return MASK_GET_IDX(msk_idx, run_start);
			}
		}

		/*
		 * we didn't find our run within the mask, or n > MASK_ALIGN,
		 * so we're going for plan B.
		 */

		/* count leading zeroes on invelld mask */
		if (~cur_msk == 0)
			clz = sizeof(cur_msk) * 8;
		else
			clz = __builtin_clzll(~cur_msk);

		/* if there aren't any runs at the end either, just continue */
		if (clz == 0)
			continue;
		/* we have a partial run at the end, so try looking ahead */
		run_start = MASK_ALIGN - clz;
		left -= clz;
		for (lookahead_idx = msk_idx + 1; lookahead_idx < msk->n_masks;
				lookahead_idx++) {			
			unsigned int s_idx, need;
			lookahead_msk = msk->data[lookahead_idx];
			/* if we're looking for free space, invert the mask */
			if (!used)
				lookahead_msk = ~lookahead_msk;
			if (unlikely(lookahead_idx == last))
				lookahead_msk &= last_msk;

			/* figure out how many consecutive bits we need here */
			need = LL_MIN(left, MASK_ALIGN);

			for (s_idx = 0; s_idx < need - 1; s_idx++)
				lookahead_msk &= lookahead_msk >> 1ULL;

			/* if first bit is not set, we've lost the run */
			if ((lookahead_msk & 1) == 0) {
				/*
				 * we've scanned this far, so we know there are
				 * no runs in the space we've lookahead-scanned
				 * as well, so skip that on next iteration.
				 */
				//ignore_msk = ~((1ULL << need) - 1);
				msk_idx = lookahead_idx;
				break;
			}

			left -= need;

			/* check if we've found what we were looking for */
			if (left == 0) {
				found = true;
				break;
			}
		}

		/* we didn't find anything, so continue */
		if (!found)
			continue;

		return MASK_GET_IDX(msk_idx, run_start);
	}
	/* we didn't find anything */
	ll_errno = used ? ENOENT : ENOSPC;
	return -1;
}

static int
find_prev(const struct ll_fbarray *arr, unsigned int start, bool used)
{
	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz,
			arr->len);
	unsigned int idx, first, first_mod;
	uint64_t ignore_msk;

	/*
	 * mask only has granularity of MASK_ALIGN, but start may not be aligned
	 * on that boundary, so construct a special mask to exclude anything we
	 * don't want to see to avoid confusing clz.
	 */
	first = MASK_LEN_TO_IDX(start);
	first_mod = MASK_LEN_TO_MOD(start);
	/* we're going backwards, so mask must start from the top */
	ignore_msk = first_mod == MASK_ALIGN - 1 ?
				UINT64_MAX : /* prevent overflow */
				~(UINT64_MAX << (first_mod + 1));

	/* go backwards, include zero */
	idx = first;
	do {
		uint64_t cur = msk->data[idx];
		int found;

		/* if we're looking for free entries, invert mask */
		if (!used)
			cur = ~cur;

		/* ignore everything before start on first iteration */
		if (idx == first)
			cur &= ignore_msk;

		/* check if we have any entries */
		if (cur == 0)
			continue;

		/*
		 * find last set bit - that will correspond to whatever it is
		 * that we're looking for. we're counting trailing zeroes, thus
		 * the value we get is counted from end of mask, so calculate
		 * position from start of mask.
		 */
		found = MASK_ALIGN - __builtin_clzll(cur) - 1;

		return MASK_GET_IDX(idx, found);
	} while (idx-- != 0); /* decrement after check  to include zero*/

	/* we didn't find anything */
	ll_errno = used ? ENOENT : ENOSPC;
	return -1;
}

static int
find_prev_n(const struct ll_fbarray *arr, unsigned int start, unsigned int n,
		bool used)
{
	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz,
			arr->len);
	unsigned int msk_idx, lookbehind_idx, first, first_mod;
	uint64_t ignore_msk;

	/*
	 * mask only has granularity of MASK_ALIGN, but start may not be aligned
	 * on that boundary, so construct a special mask to exclude anything we
	 * don't want to see to avoid confusing ctz.
	 */
	first = MASK_LEN_TO_IDX(start);
	first_mod = MASK_LEN_TO_MOD(start);
	/* we're going backwards, so mask must start from the top */
	ignore_msk = first_mod == MASK_ALIGN - 1 ?
				UINT64_MAX : /* prevent overflow */
				~(UINT64_MAX << (first_mod + 1));

	/* go backwards, include zero */
	msk_idx = first;
	do {
		uint64_t cur_msk, lookbehind_msk;
		unsigned int run_start, run_end, ctz, left;
		bool found = false;
		/*
		 * The process of getting n consecutive bits from the top for
		 * arbitrary n is a bit involved, but here it is in a nutshell:
		 *
		 *  1. let n be the number of consecutive bits we're looking for
		 *  2. check if n can fit in one mask, and if so, do n-1
		 *     lshift-ands to see if there is an appropriate run inside
		 *     our current mask
		 *    2a. if we found a run, bail out early
		 *    2b. if we didn't find a run, proceed
		 *  3. invert the mask and count trailing zeroes (that is, count
		 *     how many consecutive set bits we had starting from the
		 *     start of current mask) as k
		 *    3a. if k is 0, continue to next mask
		 *    3b. if k is not 0, we have a potential run
		 *  4. to satisfy our requirements, next mask must have n-k
		 *     consecutive set bits at the end, so we will do (n-k-1)
		 *     lshift-ands and check if last bit is set.
		 *
		 * Step 4 will need to be repeated if (n-k) > MASK_ALIGN until
		 * we either run out of masks, lose the run, or find what we
		 * were looking for.
		 */
		cur_msk = msk->data[msk_idx];
		left = n;

		/* if we're looking for free spaces, invert the mask */
		if (!used)
			cur_msk = ~cur_msk;

		/* if we have an ignore mask, ignore once */
		if (ignore_msk) {
			cur_msk &= ignore_msk;
			ignore_msk = 0;
		}

		/* if n can fit in within a single mask, do a search */
		if (n <= MASK_ALIGN) {
			uint64_t tmp_msk = cur_msk;
			unsigned int s_idx;
			for (s_idx = 0; s_idx < n - 1; s_idx++)
				tmp_msk &= tmp_msk << 1ULL;
			/* we found what we were looking for */
			if (tmp_msk != 0) {
				/* clz will give us offset from end of mask, and
				 * we only get the end of our run, not start,
				 * so adjust result to point to where start
				 * would have been.
				 */
				run_start = MASK_ALIGN -
						__builtin_clzll(tmp_msk) - n;
				return MASK_GET_IDX(msk_idx, run_start);
			}
		}

		/*
		 * we didn't find our run within the mask, or n > MASK_ALIGN,
		 * so we're going for plan B.
		 */

		/* count trailing zeroes on invelld mask */
		if (~cur_msk == 0)
			ctz = sizeof(cur_msk) * 8;
		else
			ctz = __builtin_ctzll(~cur_msk);

		/* if there aren't any runs at the start either, just
		 * continue
		 */
		if (ctz == 0)
			continue;

		/* we have a partial run at the start, so try looking behind */
		run_end = MASK_GET_IDX(msk_idx, ctz);
		left -= ctz;

		/* go backwards, include zero */
		lookbehind_idx = msk_idx - 1;

		/* we can't lookbehind as we've run out of masks, so stop */
		if (msk_idx == 0)
			break;

		do {
			const uint64_t last_bit = 1ULL << (MASK_ALIGN - 1);
			unsigned int s_idx, need;

			lookbehind_msk = msk->data[lookbehind_idx];

			/* if we're looking for free space, invert the mask */
			if (!used)
				lookbehind_msk = ~lookbehind_msk;

			/* figure out how many consecutive bits we need here */
			need = LL_MIN(left, MASK_ALIGN);

			for (s_idx = 0; s_idx < need - 1; s_idx++)
				lookbehind_msk &= lookbehind_msk << 1ULL;

			/* if last bit is not set, we've lost the run */
			if ((lookbehind_msk & last_bit) == 0) {
				/*
				 * we've scanned this far, so we know there are
				 * no runs in the space we've lookbehind-scanned
				 * as well, so skip that on next iteration.
				 */
				ignore_msk = UINT64_MAX << need;
				msk_idx = lookbehind_idx;
				break;
			}

			left -= need;

			/* check if we've found what we were looking for */
			if (left == 0) {
				found = true;
				break;
			}
		} while ((lookbehind_idx--) != 0); /* decrement after check to
						    * include zero
						    */

		/* we didn't find anything, so continue */
		if (!found)
			continue;

		/* we've found what we were looking for, but we only know where
		 * the run ended, so calculate start position.
		 */
		return run_end - n;
	} while (msk_idx-- != 0); /* decrement after check to include zero */
	/* we didn't find anything */
	ll_errno = used ? ENOENT : ENOSPC;
	return -1;
}

static int
set_used(struct ll_fbarray *arr, unsigned int idx, bool used) {
	struct used_mask *msk;
	uint64_t msk_bit = 1ULL << MASK_LEN_TO_MOD(idx);
	unsigned int msk_idx = MASK_LEN_TO_IDX(idx);
	bool already_used;
	int ret = -1;

	if (arr == NULL || idx >= arr->len) {
		ll_errno = EINVAL;
		return -1;
	}
	msk = get_used_mask(arr->data, arr->elt_sz, arr->len);
	ret = 0;

	/* prevent array from changing under us */
	ll_rwlock_write_lock(&arr->rwlock);

	already_used = (msk->data[msk_idx] & msk_bit) != 0;

	/* nothing to be done */
	if (used == already_used)
		goto out;

	if (used) {
		msk->data[msk_idx] |= msk_bit;
		arr->count++;
	} else {
		msk->data[msk_idx] &= ~msk_bit;
		arr->count--;
	}
out:
	ll_rwlock_write_unlock(&arr->rwlock);

	return ret;
}


int
ll_fbarray_set_used(struct ll_fbarray *arr, unsigned int idx)
{
	return set_used(arr, idx, true);
}

int
ll_fbarray_set_free(struct ll_fbarray *arr, unsigned int idx)
{
	return set_used(arr, idx, false);
}

int
ll_fbarray_is_used(struct ll_fbarray *arr, unsigned int idx)
{
	struct used_mask *msk;
	int msk_idx;
	uint64_t msk_bit;
	int ret = -1;

	if (arr == NULL || idx >= arr->len) {
		ll_errno = EINVAL;
		return -1;
	}

	/* prevent array from changing under us */
	ll_rwlock_read_lock(&arr->rwlock);

	msk = get_used_mask(arr->data, arr->elt_sz, arr->len);
	msk_idx = MASK_LEN_TO_IDX(idx);
	msk_bit = 1ULL << MASK_LEN_TO_MOD(idx);

	ret = (msk->data[msk_idx] & msk_bit) != 0;

	ll_rwlock_read_unlock(&arr->rwlock);

	return ret;
}

static int
fbarray_find(struct ll_fbarray *arr, unsigned int start, bool next, bool used)
{
	int ret = -1;

	if (arr == NULL || start >= arr->len) {
		ll_errno = EINVAL;
		return -1;
	}

	/* prevent array from changing under us */
	ll_rwlock_read_lock(&arr->rwlock);

	/* cheap checks to prevent doing useless work */
	if (!used) {
		if (arr->len == arr->count) {
			ll_errno = ENOSPC;
			goto out;
		}
		if (arr->count == 0) {
			ret = start;
			goto out;
		}
	} else {
		if (arr->count == 0) {
			ll_errno = ENOENT;
			goto out;
		}
		if (arr->len == arr->count) {
			ret = start;
			goto out;
		}
	}
	if (next)
		ret = find_next(arr, start, used);
	else
		ret = find_prev(arr, start, used);
out:
	ll_rwlock_read_unlock(&arr->rwlock);
	return ret;
}

int
ll_fbarray_find_next_free(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find(arr, start, true, false);
}

int
ll_fbarray_find_next_used(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find(arr, start, true, true);
}

int
ll_fbarray_find_prev_free(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find(arr, start, false, false);
}

int
ll_fbarray_find_prev_used(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find(arr, start, false, true);
}

static int
fbarray_find_n(struct ll_fbarray *arr, unsigned int start, unsigned int n,
		bool next, bool used)
{
	int ret = -1;

	if (arr == NULL || start >= arr->len || n > arr->len || n == 0) {
		ll_errno = EINVAL;
		return -1;
	}
	if (next && (arr->len - start) < n) {
		ll_errno = used ? ENOENT : ENOSPC;
		return -1;
	}
	if (!next && start < (n - 1)) {
		ll_errno = used ? ENOENT : ENOSPC;
		return -1;
	}

	/* prevent array from changing under us */
	ll_rwlock_read_lock(&arr->rwlock);

	/* cheap checks to prevent doing useless work */
	if (!used) {
		if (arr->len == arr->count || arr->len - arr->count < n) {
			ll_errno = ENOSPC;
			goto out;
		}
		if (arr->count == 0) {
			ret = next ? start: start - n + 1;
			goto out;
		}
	} else {
		if (arr->count < n) {
			ll_errno = ENOENT;
			goto out;
		}
		if (arr->count == arr->len) {
			ret = next ? start: start - n + 1;
			goto out;
		}
	}

	if (next)
		ret = find_next_n(arr, start, n, used);
	else
		ret = find_prev_n(arr, start, n, used);
out:
	ll_rwlock_read_unlock(&arr->rwlock);
	return ret;
}

int
ll_fbarray_find_next_n_free(struct ll_fbarray *arr, unsigned int start,
		unsigned int n)
{
	return fbarray_find_n(arr, start, n, true, false);
}

int
ll_fbarray_find_next_n_used(struct ll_fbarray *arr, unsigned int start,
		unsigned int n)
{
	return fbarray_find_n(arr, start, n, true, true);
}

int
ll_fbarray_find_prev_n_free(struct ll_fbarray *arr, unsigned int start,
		unsigned int n)
{
	return fbarray_find_n(arr, start, n, false, false);
}

int
ll_fbarray_find_prev_n_used(struct ll_fbarray *arr, unsigned int start,
		unsigned int n)
{
	return fbarray_find_n(arr, start, n, false, true);
}

static int
find_contig(const struct ll_fbarray *arr, unsigned int start, bool used)
{
	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz,
			arr->len);
	unsigned int idx, first, first_mod;
	unsigned int last, last_mod;
	uint64_t last_msk;
	unsigned int need_len, result = 0;

	/* array length may not be aligned, so calculate ignore mask for last
	 * mask index.
	 */
	last = MASK_LEN_TO_IDX(arr->len);
	last_mod = MASK_LEN_TO_MOD(arr->len);
	last_msk = ~(-(1ULL) << last_mod);

	first = MASK_LEN_TO_IDX(start);
	first_mod = MASK_LEN_TO_MOD(start);
	for (idx = first; idx < msk->n_masks; idx++, result += need_len) {
		uint64_t cur = msk->data[idx];
		unsigned int run_len;

		need_len = MASK_ALIGN;

		/* if we're looking for free entries, invert mask */
		if (!used)
			cur = ~cur;

		/* if this is last mask, ignore everything after last bit */
		if (idx == last)
			cur &= last_msk;

		/* ignore everything before start on first iteration */
		if (idx == first) {
			cur >>= first_mod;
			/* at the start, we don't need the full mask len */
			need_len -= first_mod;
		}

		/* we will be looking for zeroes, so invert the mask */
		cur = ~cur;

		/* if mask is zero, we have a complete run */
		if (cur == 0)
			continue;

		/*
		 * see if current run ends before mask end.
		 */
		run_len = __builtin_ctzll(cur);

		/* add however many zeroes we've had in the last run and quit */
		if (run_len < need_len) {
			result += run_len;
			break;
		}
	}
	return result;
}

static int
find_rev_contig(const struct ll_fbarray *arr, unsigned int start, bool used)
{
	const struct used_mask *msk = get_used_mask(arr->data, arr->elt_sz,
			arr->len);
	unsigned int idx, first, first_mod;
	unsigned int need_len, result = 0;

	first = MASK_LEN_TO_IDX(start);
	first_mod = MASK_LEN_TO_MOD(start);

	/* go backwards, include zero */
	idx = first;
	do {
		uint64_t cur = msk->data[idx];
		unsigned int run_len;

		need_len = MASK_ALIGN;

		/* if we're looking for free entries, invert mask */
		if (!used)
			cur = ~cur;

		/* ignore everything after start on first iteration */
		if (idx == first) {
			unsigned int end_len = MASK_ALIGN - first_mod - 1;
			cur <<= end_len;
			/* at the start, we don't need the full mask len */
			need_len -= end_len;
		}

		/* we will be looking for zeroes, so invert the mask */
		cur = ~cur;

		/* if mask is zero, we have a complete run */
		if (cur == 0)
			goto endloop;

		/*
		 * see where run ends, starting from the end.
		 */
		run_len = __builtin_clzll(cur);

		/* add however many zeroes we've had in the last run and quit */
		if (run_len < need_len) {
			result += run_len;
			break;
		}
endloop:
		result += need_len;
	} while (idx-- != 0); /* decrement after check to include zero */
	return result;
}

static int
fbarray_find_contig(struct ll_fbarray *arr, unsigned int start, bool next,
		bool used)
{
	int ret = -1;

	if (arr == NULL || start >= arr->len) {
		ll_errno = EINVAL;
		return -1;
	}

	/* prevent array from changing under us */
	ll_rwlock_read_lock(&arr->rwlock);

	/* cheap checks to prevent doing useless work */
	if (used) {
		if (arr->count == 0) {
			ret = 0;
			goto out;
		}
		if (next && arr->count == arr->len) {
			ret = arr->len - start;
			goto out;
		}
		if (!next && arr->count == arr->len) {
			ret = start + 1;
			goto out;
		}
	} else {
		if (arr->len == arr->count) {
			ret = 0;
			goto out;
		}
		if (next && arr->count == 0) {
			ret = arr->len - start;
			goto out;
		}
		if (!next && arr->count == 0) {
			ret = start + 1;
			goto out;
		}
	}

	if (next)
		ret = find_contig(arr, start, used);
	else
		ret = find_rev_contig(arr, start, used);
out:
	ll_rwlock_read_unlock(&arr->rwlock);
	return ret;
}


int
ll_fbarray_find_contig_free(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find_contig(arr, start, true, false);
}

int
ll_fbarray_find_contig_used(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find_contig(arr, start, true, true);
}

int
ll_fbarray_find_rev_contig_free(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find_contig(arr, start, false, false);
}

int
ll_fbarray_find_rev_contig_used(struct ll_fbarray *arr, unsigned int start)
{
	return fbarray_find_contig(arr, start, false, true);
}
