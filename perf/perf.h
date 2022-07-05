
#ifndef _PERF_H
#define _PERF_H


#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>

#include <limits.h>
#include <unistd.h>

#include <ll_common.h>

#include "perfsys.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _HF_DYNFILE_NONE         = 0x0,
    _HF_DYNFILE_INSTR_COUNT  = 0x1,
    _HF_DYNFILE_BRANCH_COUNT = 0x2,
    _HF_DYNFILE_BTS_TRACE     = 0x10,
    _HF_DYNFILE_INS_TRACE    = 0x20,
    _HF_DYNFILE_SOFT         = 0x40,
} dyn_file_method_t;


struct hwcnt_t {
    uint64_t cpu_instr_cnt;
    uint64_t cpu_branch_cnt;
    //uint64_t bbCnt;
    //uint64_t newBBCnt;
    //uint64_t softCntPc;
    //uint64_t softCntEdge;
    //uint64_t softCntCmp;
};

struct perf_t{

    /* For the Linux code */
    struct {
        bool        kernelOnly;
    } arch_linux;

    struct {
        dyn_file_method_t dyn_file_method;
    } feedback;

} ;

struct run_t {
    struct perf_t*  global;
    pid_t           pid;
    int             cpu_index;
    int             group_fd;
    
    int64_t      timeStartedUSecs;
    char         crashFileName[PATH_MAX];
    uint64_t     pc;
    uint64_t     backtrace;
    uint64_t     access;
    int          exception;
    //char         report[_HF_REPORT_SIZE];
    bool         mainWorker;
    unsigned     mutationsPerRun;
    //dynfile_t*   dynfile;
    bool         staticFileTryMore;
    uint32_t     fuzzNo;
    int          persistentSock;
    //runState_t   runState;
    bool         tmOutSignaled;
    //char*        args[_HF_ARGS_MAX + 1];
    int          perThreadCovFeedbackFd;
    unsigned     triesLeft;
    //dynfile_t*   current;
#if !defined(_HF_ARCH_DARWIN)
    //timer_t timerId;
#endif    // !defined(_HF_ARCH_DARWIN)
    struct hwcnt_t hw_cnt;

    struct {
        /* For Linux code */
        uint8_t* perf_mmap_buf;
        uint8_t* perf_mmap_aux;
        int      cpu_instr_fd;
        int      cpu_branch_fd;
        int      cpu_ipt_bts_fd;
    } arch_linux;
};


pid_t get_pid_byname(const char *task_name);

bool ll_arch_perf_create(struct run_t *run, dyn_file_method_t method, int *perf_fd);

bool ll_arch_perf_init(struct run_t *run);

bool ll_arch_perf_open(struct run_t *run);

bool ll_arch_perf_enable(struct run_t* run);

void ll_arch_perf_analyze(struct run_t *run);

void ll_arch_perf_close(struct run_t* run);

static void __ll_always_inline
ll_arch_perf_run_set(struct run_t * run, pid_t pid, int cpu_index, int group_fd) {
    run->pid = pid;
    run->cpu_index = cpu_index;
    run->group_fd = group_fd;
}

static bool __ll_always_inline
ll_arch_perf_vt_run_set(struct run_t * run, const char *task_name) {
    if (unlikely(run == NULL)) {
        return false;
    }
    
    pid_t proc;
    if (task_name == NULL) {
        ll_arch_perf_run_set(run, -1, -1, -1);
    } else {
        if (unlikely((proc = get_pid_byname(task_name)) == -1)) {
            return false;
        }
        ll_arch_perf_run_set(run, proc, -1, -1);
    }
    return true;
}


#ifdef __cplusplus
}
#endif

#endif //_PERF_H