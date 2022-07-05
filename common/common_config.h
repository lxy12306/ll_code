#ifndef _COMMON_CONFIG_
#define _COMMON_CONFIG_

/****** library defines ********/

/* EAL defines */
#define LL_MAX_HEAPS 32
#define LL_MAX_MEMSEG_LISTS 128

/*** NUMA SET ***/
#define LL_MAX_NUMA_NODES 32

/*** lcore defines ***/
#define LL_MAX_LCORE 2
#define LL_LCORE_ID_ANY ((unsigned)(-1))

enum ll_iova_mode {
	LL_IOVA_DC = 0,	/* Don't care mode */
	LL_IOVA_PA = (1 << 0), /* DMA using physical address */
	LL_IOVA_VA = (1 << 1)  /* DMA using virtual address */
};

/*** mempool defines ***/
#define LL_MEMPOOL_CACHE_MAX_SIZE 512

#endif