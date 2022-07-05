#ifndef _HF_LINUX_INTEL_PT_H_
#define _HF_LINUX_INTEL_PT_H_

#include <ll_common.h>


#ifndef BIT
#define BIT(nr) (1UL << (nr))
#endif


//IA32_RTIT_CTL MSR bit set

#define PT_RTIT_CTL_TRACEEN         BIT(0) //pt trace enbale 
#define PT_RTIT_CTL_CYCLEACC        BIT(1)
#define PT_RTIT_CTL_OS              BIT(2)
#define PT_RTIT_CTL_USR             BIT(3)
#define PT_RTIT_CTL_PWR_EVT_EN      BIT(4)
#define PT_RTIT_CTL_FUP_ON_PTW      BIT(5)
#define PT_RTIT_CTL_CR3EN           BIT(7)
#define PT_RTIT_CTL_TOPA            BIT(8)
#define PT_RTIT_CTL_MTC_EN          BIT(9)
#define PT_RTIT_CTL_TSC_EN          BIT(10)
#define PT_RTIT_CTL_DISRETC         BIT(11) //enable RET compression



void ll_arch_pt_analyze(struct run_t* run);


/**
 * stop intel pt
 * 
 * @param run
 *  now run
 *  
 */
void ll_arch_pt_stop(struct run_t* run);

/**
 * init cpu info
 */
void
__pt_cpu_init(void);


#endif /* _HF_LINUX_INTEL_PT_H_ */



