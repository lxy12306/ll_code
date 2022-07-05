#include <sys/time.h>
#include <sys/resource.h>
#include <inttypes.h>


#include <ll_log.h>
#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_memory.h>
#include <ll_fbarray.h>

#include "eal_memory_private.h"
#include "eal_memory.h"

int
ll_memseg_walk_thread_unsafe(ll_memseg_walk_t func, void *arg) {
    struct ll_mem_config * mcfg = ll_get_mem_config();
    int i, ms_idx, ret = 0;
    
    for (i = 0; i < LL_MAX_MEMSEG_LISTS; ++i) {
        struct ll_memseg_list *msl = &mcfg->memsegs[i];
        const struct ll_memseg *ms;
        struct ll_fbarray *arr;
        
        if (msl->memseg_arr.count == 0) {
            continue;
        }

        arr = &msl->memseg_arr;
        
        ms_idx = ll_fbarray_find_next_used(arr, 0);
        while(ms_idx >= 0) {
            ms = ll_fbarray_get(arr, ms_idx);
            ret = func(msl, ms, arg);
            if (ret) {
                return ret;
            }
            ms_idx = ll_fbarray_find_next_used(arr, ms_idx + 1);
        }
    }
    return 0;
}

int
ll_memseg_walk(ll_memseg_walk_t func, void *arg) {
    int ret = 0;
    ll_mcfg_mem_read_lock();
    ret = ll_memseg_walk_thread_unsafe(func, arg);
    ll_mcfg_mem_read_unlock();
    return ret;
}

int ll_memseg_contig_walk_thread_unsafe(ll_memseg_contig_walk_t func, void *arg) {

    struct ll_mem_config * mcfg = ll_get_mem_config();
    int i, ms_idx, ret = 0;

    for (i = 0; i < LL_MAX_MEMSEG_LISTS; ++i) {
        struct ll_memseg_list *msl = &mcfg->memsegs[i];
        const struct ll_memseg *ms;
        struct ll_fbarray *arr;
        
        if (msl->memseg_arr.count == 0) {
            continue;
        }

        arr = &msl->memseg_arr;
        
        ms_idx = ll_fbarray_find_next_used(arr, 0);
        while(ms_idx >= 0) {
            int n_segs;
            size_t len;

            ms = ll_fbarray_get(arr, ms_idx);

            n_segs = ll_fbarray_find_contig_used(arr,ms_idx);
            len = n_segs * msl->page_sz;

            ret = func(msl, ms, len, arg);
            if (ret) {
                return ret;
            }
            ms_idx = ll_fbarray_find_next_used(arr, ms_idx + n_segs);
        }
    }
    return 0;
}

int 
ll_memseg_contig_walk(ll_memseg_contig_walk_t func, void *arg) {
    int ret = 0;
    ll_mcfg_mem_read_lock();
    ret = ll_memseg_contig_walk_thread_unsafe(func, arg);
    ll_mcfg_mem_read_unlock();
    return ret;
}

int ll_memseg_list_walk_thread_unsafe(ll_memseg_list_walk_t func, void *arg) {

    struct ll_mem_config *mcfg = ll_get_mem_config();
    int i, ret = 0;
    
    for (i = 0; i < LL_MAX_MEMSEG_LISTS; i++) {
        struct ll_memseg_list *msl = &mcfg->memsegs[i];
        
        if (msl->base_va == NULL) {
            continue;
        }

        ret = func(msl, arg);
        if (ret) {
            return ret;
        }
    }
    
    return 0; 
}

int
ll_memseg_list_walk(ll_memseg_list_walk_t func, void *arg) {
    int ret = 0;
    ll_mcfg_mem_read_lock();
    ret = ll_memseg_list_walk_thread_unsafe(func, arg);
    ll_mcfg_mem_read_unlock();
    
    return ret;
}

static int __ll_unused
memseg_primary_init(void)
{
	return eal_dynmem_memseg_lists_init();
}

static int __ll_unused
memseg_secondary_init(void)
{
	return eal_dynmem_memseg_lists_init();
}

int
ll_eal_memseg_init(void)
{
	/* increase rlimit to maximum */
	struct rlimit lim;

#ifndef LL_EAL_NUMA_AWARE_HUGEPAGES
	const struct internal_config *internal_conf = ll_eal_get_internal_configuration();
#endif
	if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
		/* set limit to maximum */
		lim.rlim_cur = lim.rlim_max;

		if (setrlimit(RLIMIT_NOFILE, &lim) < 0) {
			LL_LOG(DEBUG, EAL, "Setting maximum number of open files failed: %s\n",
					strerror(errno));
		} else {
			LL_LOG(DEBUG, EAL, "Setting maximum number of open files to %"
					PRIu64 "\n",
					(uint64_t)lim.rlim_cur);
		}
	} else {
		LL_LOG(ERR, EAL, "Cannot get current resource limits\n");
	}
#ifndef LL_EAL_NUMA_AWARE_HUGEPAGES
	if (!internal_conf->legacy_mem && ll_socket_count() > 1) {
		LL_LOG(WARNING, EAL, "DPDK is running on a NUMA system, but is compiled without NUMA support.\n");
		LL_LOG(WARNING, EAL, "This will have adverse consequences for performance and usability.\n");
		LL_LOG(WARNING, EAL, "Please use --\"OPT_LEGACY_MEM\" option, or recompile with NUMA support.\n");
	}
#endif

	return ll_eal_process_type() == LL_PROC_PRIMARY ?
#ifndef LL_ARCH_64
			memseg_primary_init_32() :
#else
			memseg_primary_init() :
#endif
			memseg_secondary_init();
}

int
ll_eal_hugepage_init() { 
    const struct internal_config *internal_conf =
		ll_eal_get_internal_configuration();

	return internal_conf->legacy_mem ?
			eal_legacy_hugepage_init() :
			eal_dynmem_hugepage_init();
}



//init memory subsystem
int
ll_eal_memory_init(void) {
	struct ll_mem_config *mcfg = ll_get_mem_config();
    const struct internal_config *internal_conf = ll_eal_get_internal_configuration();
    
    int ret;
    LL_LOG(DEBUG, EAL, "Setting up physically contiguous memory...\n");
    
    if (!mcfg) {
        return -1;
    }

    ll_mcfg_mem_read_lock();

    ll_eal_memseg_init();

    ll_eal_hugepage_init();

    ll_mcfg_mem_read_unlock();

    return 0;
}