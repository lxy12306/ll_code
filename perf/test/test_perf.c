#include <unistd.h>

#include <ll_log.h>
#include <ll_memory.h>

#include "../perf.h"

#define EXAMPLE_PATH "/home/lxy/ubantu/code/perf/test/example"

int main()
{
#if 0
    int ret = execl(EXAMPLE_PATH, EXAMPLE_PATH,(char*)0);
    if (ret == -1) {
        return -1;
    }
#endif
    ll_log_set_global_level(LL_LOG_DEBUG);
    ll_log_set_level(LL_LOGTYPE_PERF,LL_LOG_DEBUG);

    system("sudo sysctl -w kernel.perf_event_mlock_kb=3000000");

    struct run_t *run = ll_zmalloc("1", sizeof(struct run_t), 0);
    ll_arch_perf_init(run);

    run->global->feedback.dyn_file_method = _HF_DYNFILE_INS_TRACE;

    ll_arch_perf_vt_run_set(run, "example");

    ll_arch_perf_open(run);
    
    ll_arch_perf_enable(run);

    usleep(10000);
    
    ll_arch_perf_analyze(run);
    
    ll_arch_perf_close(run);

    system("sudo sysctl -w kernel.perf_event_mlock_kb=516");
    return 0;
}