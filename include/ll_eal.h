#ifndef _LL_EAL_H_
#define _LL_EAL_H_


enum ll_proc_type_t {
    LL_PROC_AUTO = -1, //allow auto-detection of primary/secondary
    LL_PROC_PRIMARY = 0, //set to zero, so primary is the default
    LL_PROC_SECONDARY,
    
    LL_PROC_INVALID,
};

/**
 * Get the process type in a multi-process setup
 *
 * @return
 *   The process type
 */
enum ll_proc_type_t ll_eal_process_type(void);

/**
 * init lcore runtime
 *
 * @return
 *   0 : init sucessful
 */
int
ll_eal_cpu_init(void);
#endif