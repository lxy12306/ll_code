
#ifndef _EAL_HUGEPAGES_H_
#define _EAL_HUGEPAGES_H_

#include <stdint.h>
#include <limits.h>

#define MAX_HUGEPAGE_PATH PATH_MAX

#define MAX_HUGEPAGE_SIZES 3  /**< support up to 3 page sizes */

/**
 * Structure used to store information about hugepages that we mapped
 * through the files in hugetlbfs.
 */
struct hugepage_file {
    void *orig_Va; //virtual addr of first mmap()
    void *final_va; //virtual addr of 2nd mmap()
    uint64_t physaddr; //physical addr
    size_t size; //the page size
    int socket_id; //NUMA socket ID
    int file_id; //the '%d' in HUGEFILE_FMT
    char filepath[MAX_HUGEPAGE_PATH];
};


/*
 * internal configuration structure for the number, size and
 * mount points of hugepages
 */
struct hugepage_info {
	uint64_t hugepage_sz;   /**< size of a huge page */
	char hugedir[MAX_HUGEPAGE_PATH];    /**< dir where hugetlbfs is mounted */
	uint32_t num_pages[LL_MAX_NUMA_NODES];
	/**< number of hugepages of that size on each socket */
	int lock_descriptor;    /**< file descriptor for hugepage dir */
};


#endif //_EAL_HUGEPAGES_H_
