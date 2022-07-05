#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/syscall.h>

#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>


#include <ll_log.h>
#include <ll_errno.h>
#include <ll_print_c.h>
#include <ll_memory.h>

#include <ll_pmem.h>
#include "../perf.h"

#include "../util/intel_pt_decoder/intel_pt_insn_decoder.h"


#define PATH "/media/lxy/SSD_1T/BaiduNetdiskWorkspace/ubantu/code/process/test/hello"

int main() {
    
    uint64_t next;
    pid_t pid = ll_pmem_ptrace_process(PATH,"hello");
    struct ll_process_mem_maps *maps = NULL;
    ll_log_set_level(LL_LOGTYPE_PERF,LL_LOG_DEBUG);

    maps = ll_pmem_get_proc_maps(pid);
    struct insn *p = ll_pmem_analyze_function(maps, (uint64_t)maps->text[0].begin, &next);

    ll_pmem_printf_insn_list(stdout, p);


    ptrace(PTRACE_CONT, pid, NULL, NULL);

    ll_pmem_free_insn_list(p);
    ll_pmem_free_proc_maps(maps);

    return 0;
}