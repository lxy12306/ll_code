#include <stdbool.h>

#include <ll_common.h>
#include <ll_config.h>

#include <ll_fbarray.h>
#include <ll_memory.h>


bool
eal_memalloc_is_contig(const struct ll_memseg_list *msl, void *start, size_t len) {
    void *end, *aligned_start, *aligned_end;
    size_t pgsz = (size_t)msl->page_sz;
    const struct ll_memseg *ms;

    //for iova_va, it's alway contiguous
    if (ll_eal_iova_mode() == LL_IOVA_VA && !msl->external) {
        return true;
    }

    end = LL_PTR_ADD(start, len);
    
    //for nohuge, we check pagemap, otherwise check memseg
    if (ll_eal_has_hugepages()) {
        int start_seg, end_seg, cur_seg;
		ll_iova_t cur, expected;

        aligned_start = LL_PTR_ALIGN_FLOOR(start, pgsz);
		aligned_end = LL_PTR_ALIGN_CEIL(end, pgsz);

        start_seg = LL_PTR_DIFF(aligned_start, msl->base_va) / pgsz;
		end_seg = LL_PTR_DIFF(aligned_end, msl->base_va) / pgsz;

        //if start and end are on the same page, bail out early
        if (LL_PTR_DIFF(aligned_start, aligned_end)  ==  pgsz) {
            return true;
        }


        /* skip first iteration */
		ms = ll_fbarray_get(&msl->memseg_arr, start_seg);
		cur = ms->iova;
		expected = cur + pgsz;

		/* if we can't access IOVA addresses, assume non-contiguous */
		if (cur == LL_BAD_IOVA)
			return false;

		for (cur_seg = start_seg + 1; cur_seg < end_seg;
				cur_seg++, expected += pgsz) {
			ms = ll_fbarray_get(&msl->memseg_arr, cur_seg);

			if (ms->iova != expected)
				return false;
		}
        
    } else {
        ll_iova_t cur, expected;

        aligned_start = LL_PTR_ALIGN_FLOOR(start, pgsz);
		aligned_end = LL_PTR_ALIGN_CEIL(end, pgsz);

        //if start and end are on the same page, bail out early
        if (LL_PTR_DIFF(aligned_start, aligned_end)  ==  pgsz) {
            return true;
        }

        //
        cur = ll_mem_virt2iova(aligned_alloc);
        if (cur == LL_BAD_IOVA) {
            return false;
        }
        expected = cur + pgsz;
        aligned_start = LL_PTR_ADD(aligned_start, pgsz);
        
        while (aligned_start < aligned_end) {
			cur = ll_mem_virt2iova(aligned_start);
			if (cur != expected)
				return false;
			aligned_start = LL_PTR_ADD(aligned_start, pgsz);
			expected += pgsz;
		}
    }

    return true;
}

void *
malloc_socket(const char *type, size_t size, unsigned align,
    int socket_arg, const bool trace_ena) {
    
    void *ptr = NULL;
    
    if (size == 0 || (align && ll_is_power_of_2(align))) {
        return NULL;
    }

    return NULL;
}