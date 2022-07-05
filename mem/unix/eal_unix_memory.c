#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>

#include <ll_common.h>
#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_errno.h>
#include <ll_log.h>
#include <ll_fbarray.h>
#include <ll_memory.h>


#include "eal_hugepages.h"
#include "../eal_memory.h"

#define PFN_MASK_SIZE 8

#define EAL_DONTDUMP MADV_DONTDUMP
#define EAL_DODUMP   MADV_DODUMP

static void *next_baseaddr;
static uint64_t system_page_sz;

/*** mem_info ***/
size_t
ll_mem_page_size(void) {
	static size_t page_size = 0;

	if (!page_size)
		page_size = sysconf(_SC_PAGESIZE);

	return page_size;
}

/*** mem_map ***/

static void *
mem_map(void *requested_addr, size_t size, int prot, int flags,
	int fd, uint64_t offset)
{
	void *virt = mmap(requested_addr, size, prot, flags, fd, offset);
	if (virt == MAP_FAILED) {
		LL_LOG(DEBUG, EAL,
		    "Cannot mmap(%p, 0x%zx, 0x%x, 0x%x, %d, 0x%"PRIx64"): %s\n",
		    requested_addr, size, prot, flags, fd, offset,
		    strerror(errno));
		ll_errno = errno;
		return NULL;
	}
	return virt;
}

void *
eal_mem_reserve(void *requested_addr, size_t size, int flags)
{
	int sys_flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if (flags & EAL_RESERVE_HUGEPAGES) {
#ifdef MAP_HUGETLB
		sys_flags |= MAP_HUGETLB;
#else
		ll_errno = ENOTSUP;
		return NULL;
#endif
	}

	if (flags & EAL_RESERVE_FORCE_ADDRESS)
		sys_flags |= MAP_FIXED;

        
	return mem_map(requested_addr, size, PROT_NONE, sys_flags, -1, 0);
}

int
eal_mem_unmap(void *virt, size_t size)
{
	int ret = munmap(virt, size);
	if (ret < 0) {
		LL_LOG(DEBUG, EAL, "Cannot munmap(%p, 0x%zx): %s\n",
			virt, size, strerror(errno));
		ll_errno = errno;
	}
	return ret;
}

void
eal_mem_free(void *virt, size_t size)
{
	eal_mem_unmap(virt, size);
}

int
eal_mem_set_dump(void *virt, size_t size, bool dump)
{
	int flags = dump ? EAL_DODUMP : EAL_DONTDUMP;
	int ret = madvise(virt, size, flags);
	if (ret) {
		LL_LOG(DEBUG, EAL, "madvise(%p, %#zx, %d) failed: %s\n",
				virt, size, flags, strerror(ll_errno));
		ll_errno = errno;
	}
	return ret;
}

#define MAX_MMAP_WITH_DEFINED_ADDR_TRIES 5
void *
eal_get_virtual_area(void *requested_addr, size_t *size,
	size_t page_sz, int flags, int reserve_flags) {

	bool addr_is_hint, allow_shrink, unmap, no_align;
	uint64_t map_sz;
	void *mapped_addr, *aligned_addr;
	uint8_t try;
	struct internal_config *internal_conf = ll_eal_get_internal_configuration();

	if (system_page_sz == 0) {
		system_page_sz = ll_mem_page_size();
	}

	LL_LOG(DEBUG, EAL, "Ask a virtual area of 0x%zx bytes\n", *size);

	addr_is_hint = (flags & EAL_VIRTUAL_AREA_ADDR_IS_HINT) > 0;
	allow_shrink = (flags & EAL_VIRTUAL_AREA_ALLOW_SHRINK) > 0;
	unmap = (flags & EAL_VIRTUAL_AREA_UNMAP);
	
	if (next_baseaddr == NULL && internal_conf->base_virtaddr != 0 && ll_eal_process_type() == LL_PROC_PRIMARY) {
		next_baseaddr = (void *)internal_conf->base_virtaddr;
	}

	if (requested_addr == NULL && next_baseaddr != NULL) {
		requested_addr = next_baseaddr;
		requested_addr = LL_PTR_ALIGN(requested_addr, page_sz);
		addr_is_hint = true;
	}


	/* we don't need alignment of resulting pointer in the following cases:
	 *
	 * 1. page size is equal to system size
	 * 2. we have a requested address, and it is page-aligned, and we will
	 *    be discarding the address if we get a different one.
	 *
	 * for all other cases, alignment is potentially necessary.
	 */
	no_align = (requested_addr != NULL && requested_addr == LL_PTR_ALIGN(requested_addr, page_sz) && !addr_is_hint) ||
		page_sz == system_page_sz;

	do {
		map_sz = no_align ? *size : *size + page_sz;
		if (map_sz > SIZE_MAX) {
			LL_LOG(ERR, EAL, "Map size too big\n");
			ll_errno = E2BIG;
			return NULL;
		}
		mapped_addr = eal_mem_reserve(requested_addr, (size_t)map_sz, reserve_flags);
		if ((mapped_addr == NULL) && allow_shrink) {
			*size -= page_sz;
		}

		if ((mapped_addr != NULL) && addr_is_hint && (mapped_addr != requested_addr)) {
			try++;
			next_baseaddr = LL_PTR_ADD(next_baseaddr, page_sz);
			if (try <= MAX_MMAP_WITH_DEFINED_ADDR_TRIES) {
				//hint was not used. Try with another offset

				eal_mem_free(mapped_addr, map_sz);
				mapped_addr = NULL;
				requested_addr = next_baseaddr;
			}
		}
	} while ((allow_shrink || addr_is_hint) && (mapped_addr == NULL) && (*size > 0));
	
	/* align resulting address - if map failed, we will ignore the value
	 * anyway, so no need to add additional checks.
	 */
	aligned_addr = no_align ? mapped_addr : LL_PTR_ALIGN(mapped_addr, page_sz);
	
	if (*size == 0) {
		LL_LOG(ERR, EAL, "Cannot get a virtual area of any size: %s\n",
			ll_strerror(ll_errno));
		return NULL;
	} else if (mapped_addr == NULL) {
		LL_LOG(ERR, EAL, "Cannot get a virtual area: %s\n", ll_strerror(ll_errno));
		return NULL;
	} else if (requested_addr != NULL && !addr_is_hint && aligned_addr != requested_addr) {
		LL_LOG(ERR, EAL, "Cannot get a virtual area at requested address: %p (got %p)\n",
			requested_addr, aligned_addr);
		eal_mem_free(mapped_addr, map_sz);
		ll_errno = EADDRNOTAVAIL;
		return NULL;
	} else if (requested_addr != NULL && addr_is_hint && aligned_addr != requested_addr) {
		/*
		 * demote this warning to debug if we did not explicitly request
		 * a base virtual address.
		 */
		if (internal_conf->base_virtaddr != 0) {
			LL_LOG(WARNING, EAL, "WARNING! Base virtual address hint (%p != %p) not respected!\n",
				requested_addr, aligned_addr);
			LL_LOG(WARNING, EAL, "   This may cause issues with mapping memory into secondary processes\n");
		} else {
			LL_LOG(DEBUG, EAL, "WARNING! Base virtual address hint (%p != %p) not respected!\n",
				requested_addr, aligned_addr);
			LL_LOG(DEBUG, EAL, "   This may cause issues with mapping memory into secondary processes\n");
		}
	} else if (next_baseaddr != NULL) {
		next_baseaddr = LL_PTR_ADD(aligned_addr, *size);
	}

	LL_LOG(DEBUG, EAL, "Virtual area found at %p (size = 0x%zx)\n", aligned_addr, *size);
	
	if (unmap) {
		eal_mem_free(mapped_addr, map_sz);
	} else if (!no_align) {
		void *map_end, *aligned_end;
		size_t before_len, after_len;

		/* when we reserve space with alignment, we add alignment to
		 * mapping size. On 32-bit, if 1GB alignment was requested, this
		 * would waste 1GB of address space, which is a luxury we cannot
		 * afford. so, if alignment was performed, check if any unneeded
		 * address space can be unmapped back.
		 */

		map_end = LL_PTR_ADD(mapped_addr, (size_t)map_sz);
		aligned_end = LL_PTR_ADD(aligned_addr, *size);

		/* unmap space before aligned mmap address */
		before_len = LL_PTR_DIFF(aligned_addr, mapped_addr);
		if (before_len > 0)
			eal_mem_free(mapped_addr, before_len);

		/* unmap space after aligned end mmap address */
		after_len = LL_PTR_DIFF(map_end, aligned_end);
		if (after_len > 0)
			eal_mem_free(aligned_end, after_len);
	}

	if (!unmap) {
		/* Exclude these pages from a core dump. */
		eal_mem_set_dump(aligned_addr, *size, false);
	}

    return mapped_addr;
}

//get physical address of any mapped virtual address in the current process 
ll_phys_addr_t
ll_mem_vir2phy(const void *virtaddr) {

	int fd, ret;
	int page_size;
	unsigned long virt_pfn;
	uint64_t page, physaddr;
	off_t offset;

	page_size = getpagesize();

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0) {
		LL_LOG(INFO, EAL, "%s(): cannot open /proc/self/pagemap: %s\n",
			__func__, strerror(errno));
		return LL_BAD_IOVA;
	}

	virt_pfn = (unsigned long)virtaddr / page_size;
	offset = sizeof(uint64_t) *virt_pfn;
	if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
		LL_LOG(INFO, EAL, "%s(): seek error in /proc/self/pagemap: %s\n",
			__func__, strerror(errno));
		close(fd);
		return LL_BAD_IOVA;
	}

	ret = read(fd, &page, PFN_MASK_SIZE);
	close(fd);
	if (ret < 0) {
		LL_LOG(INFO, EAL, "%s(): cannot read /proc/self/pagemap: %s\n",
			__func__, strerror(errno));
		return LL_BAD_IOVA;
	} else if (ret != PFN_MASK_SIZE) {
		LL_LOG(INFO, EAL, "%s(): cannot read %d bytes from /proc/self/pagemap"
				"but expected %d:\n",
				__func__, ret, PFN_MASK_SIZE);
		return LL_BAD_IOVA;
	}

	if ((page & ((uint64_t)(1ULL<<63))) == 0) {
		return LL_BAD_IOVA;
	}
	
	if ((page & 0x7fffffffffffffULL) == 0) {
		return LL_BAD_IOVA;
	}

	physaddr = ((page & 0x7fffffffffffffULL) * page_size) + ((unsigned long)virtaddr % page_size);
	
	return physaddr;
}


ll_iova_t
ll_mem_virt2iova(const void *virt) {
	enum ll_iova_mode iova_mode = ll_mcfg_iova_mode();
	switch (iova_mode)
	{
	case LL_IOVA_DC:
	case LL_IOVA_VA:
		return (ll_iova_t)virt;
	
	case LL_IOVA_PA:
		return (ll_iova_t)ll_mem_vir2phy(virt);
	default:
		break;
	}
	return LL_BAD_IOVA;
}

int
eal_memseg_list_init_named(struct ll_memseg_list *msl, const char *name,
	uint64_t page_sz, int n_segs, int socket_id, bool heap) {
	
	if (ll_fbarray_init(&msl->memseg_arr, name, n_segs, sizeof(struct ll_memseg))) {
		LL_LOG(ERR, EAL, "Cannot allocate memseg list: %s\n", ll_strerror(ll_errno));
	}

	msl->page_sz = page_sz;
	msl->socket_id = socket_id;
	msl->base_va = NULL;
	msl->heap = heap;

	LL_LOG(DEBUG, EAL,
		"Memseg list allocated at socket %i, page size 0x%"PRIx64"kB\n",
		socket_id, page_sz >> 10);
	
	return 0;
}

int
eal_memseg_list_init(struct ll_memseg_list *msl, uint64_t page_sz,
		int n_segs, int socket_id, int type_msl_idx, bool heap)
{
	char name[LL_FBARRAY_NAME_LEN];

	snprintf(name, sizeof(name), MEMSEG_LIST_FMT, page_sz >> 10, socket_id,
		 type_msl_idx);

	return eal_memseg_list_init_named(
		msl, name, page_sz, n_segs, socket_id, heap);
}

int
eal_memseg_list_alloc(struct ll_memseg_list *msl, int reserve_flags) {
	size_t page_sz, mem_sz;
	void *addr;

	page_sz = msl->page_sz;
	mem_sz = page_sz * msl->memseg_arr.len;
	
	addr = eal_get_virtual_area(
		msl->base_va, &mem_sz, page_sz, 0, reserve_flags
	);
	if (addr == NULL) {
		return -1;
	}
	msl->base_va = addr;
	msl->len = mem_sz;
	
	LL_LOG(DEBUG, EAL, "VA reserved for memseg list at %p, size %zx\n", addr, mem_sz);
	
	return 0;
}

void 
eal_memseg_list_populate(struct ll_memseg_list *msl, void *addr, int n_segs) {
	size_t page_sz = msl->page_sz;
	int i;
	struct ll_fbarray *arr = &msl->memseg_arr;
	
	for (i = 0; i < n_segs; ++i) {
		struct ll_memseg *ms = ll_fbarray_get(arr, i);
		
		if (ll_eal_iova_mode() == LL_IOVA_VA) {
			ms->iova = (uintptr_t)addr;
		} else {
			ms->iova = LL_BAD_IOVA;
		}

		ms->addr = addr;
		ms->hugepage_sz = page_sz;
		ms->socket_id = 0;
		ms->len = page_sz;
		
		ll_fbarray_set_used(arr, i);

		addr = LL_PTR_ADD(addr, page_sz);
	}
}

int
eal_dynmem_memseg_lists_init(void) {
	struct internal_config *internal_conf = ll_eal_get_internal_configuration();
	struct ll_mem_config *mcfg = ll_get_mem_config();

	//no-huge does ont need this at all
	if (internal_conf->no_hugetlbfs) {
		return 0;
	}
	struct memtype {
		uint64_t page_sz;
		int socket_id;
	} *memtypes = NULL;
	unsigned int n_memtypes, cur_type;

	return 1;
}


/*
 * Prepare physical memory mapping: fill configuration structure with
 * these infos, return 0 on success.
 *  1. map N huge pages in separate files in hugetlbfs
 *  2. find associated physical addr
 *  3. find associated NUMA socket ID
 *  4. sort all huge pages by physical address
 *  5. remap these N huge pages in the correct order
 *  6. unmap the first mapping
 *  7. fill memsegs in configuration with contiguous zones
 */
int
eal_legacy_hugepage_init(void) {
	struct ll_mem_config *mcfg;
	struct hugepage_file *hugepage = NULL, *tmp_hp = NULL;
	struct hugepage_info used_hp[MAX_HUGEPAGE_SIZES];

	struct internal_config *internal_conf = ll_eal_get_internal_configuration();
	
	uint64_t memory[LL_MAX_NUMA_NODES];
	void *addr = NULL;

	ll_memset(used_hp, 0, sizeof(used_hp));

	mcfg = ll_get_mem_config();

	// if hugetlbfs is disabled
	if (internal_conf->no_hugetlbfs) {
		void *prealloc_addr;
		size_t mem_sz;
		struct ll_memseg_list *msl;
		int n_segs, fd, flags;
#ifdef MEMFD_SUPPORTED
		int memfd;
#endif
		uint64_t page_sz;
		//nohuge mode is legacy mode
		internal_conf->legacy_mem = 1;

		//nohuge mode is single-file segments mode
		internal_conf->single_file_segments = 1;
		
		//create a memseg list
		msl = &mcfg->memsegs[0];

		mem_sz = internal_conf->memory;
		page_sz = (1UL<<12);
		n_segs = mem_sz / page_sz;

		if (eal_memseg_list_init_named(msl, "nohugemem", page_sz, n_segs, 0, true)) {
			return -1;
		}

		//set up parameters for anonymous mmap
		fd = -1;
		flags = MAP_PRIVATE;
#ifdef MAP_ANONYMOUS
		flags |= MAP_ANONYMOUS;
#endif
		/* preallocate address space for the memory, so that it can be
		 * fit into the DMA mask.
		 */
		if (eal_memseg_list_alloc(msl, 0)) {
			LL_LOG(ERR, EAL, "Cannot preallocate VA space for hugepage memory\n");
			return -1;
		}

		prealloc_addr = msl->base_va;
		addr = mmap(prealloc_addr, mem_sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, 0);
		if (addr == MAP_FAILED && addr != prealloc_addr) {
			LL_LOG(ERR, EAL, "%s: mmap() failed: %s\n", __func__,
					strerror(errno));
			munmap(addr, mem_sz);
			return -1;
		}

		eal_memseg_list_populate(msl, addr, n_segs);
		return 0;
	}

	return -1;
}
