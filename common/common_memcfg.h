#ifndef EAL_MEMCFG_H
#define EAL_MEMCFG_H

#include <ll_pause.h>
#include <ll_spinlock.h>

/**
 * Memory configuration shared across multiple processes.
 */
struct ll_mem_config
{
	volatile uint32_t magic;   /**< Magic number - sanity check. */
    /* memory topology */
	uint32_t nchannel;    /**< Number of channels (0 if unknown). */
	uint32_t nrank;       /**< Number of ranks (0 if unknown). */

    
};


#endif
