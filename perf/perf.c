#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <sys/mman.h>




//#include <intel_pt.h>

#include <ll_log.h>
#include <ll_errno.h>
#include <ll_file.h>
#include <ll_memory.h>
#include <ll_atomic.h>

#include "perf.h"
#include "intel_pt.h"
#include "util/intel_pt_decoder/intel_pt_log.h"

#define _TEMP_PAGE_SIZE 4096

#define _HF_PERF_MAP_SIZE LL_ALIGN(sizeof(struct perf_event_mmap_page),4096)
#define _HF_PERF_AUX_PAGE_NUM 128 * 2

static int32_t perf_intel_pt_perf_type  = -1;
static int32_t perf_intel_bts_perf_type = -1;


#define BUF_SIZE 1024
#define TASK_NAME_MAX 512
pid_t get_pid_byname(const char* task_name)
{
    DIR *dir;
    struct dirent *ptr;
    FILE *fp;
    char filepath[TASK_NAME_MAX];//大小随意，能装下cmdline文件的路径即可
    char cur_task_name[TASK_NAME_MAX];//大小随意，能装下要识别的命令行文本即可
    char buf[BUF_SIZE];
    dir = opendir("/proc"); //打开路径
    pid_t pid = -1;
    if (NULL != dir)
    {
        while ((ptr = readdir(dir)) != NULL) //循环读取路径下的每一个文件/文件夹
        {
            //如果读取到的是"."或者".."则跳过，读取到的不是文件夹名字也跳过
            if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))            
                continue;
            if (DT_DIR != ptr->d_type)
                continue;

            sprintf(filepath, "/proc/%s/status", ptr->d_name);//生成要读取的文件的路径
            fp = fopen(filepath, "r\n");//打开文件
            if (NULL != fp)
            {
                if( fgets(buf, BUF_SIZE-1, fp)== NULL ){
                    fclose(fp);
                    continue;
                }
                fclose(fp);
                sscanf(buf, "%*s %s", cur_task_name);
                //LL_LOG(DEBUG, PERF, "%s\n", cur_task_name);
                //如果文件内容满足要求则打印路径的名字（即进程的PID）
                if (!strcmp(task_name, cur_task_name)) {
                    pid = atoi(ptr->d_name);
                    break;
                }
            }
        }
        closedir(dir);//关闭路径
    }
    return pid;
}


bool ll_arch_perf_create(struct run_t *run, dyn_file_method_t method, int *perf_fd) {
    
    size_t pg_sz = ll_mem_page_size();

    struct perf_event_attr pe;
    if ((method & _HF_DYNFILE_BTS_TRACE) &&  perf_intel_bts_perf_type== -1) {
        LL_LOG(INFO, PERF, "Intel BTS events (new type) are not supported on this platform\n");
    }

    if ((method & _HF_DYNFILE_INS_TRACE) && perf_intel_pt_perf_type == -1) {
        LL_LOG(INFO, PERF, "Intel PT events are not supported on this platform\n");
    }

    //init perf_event_attr
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.size = sizeof(struct perf_event_attr);
    pe.disabled = 1; //in the init,disable the events

    if (run->global->arch_linux.kernelOnly) {
        pe.exclude_user = 1;
    } else {
        pe.exclude_kernel = 1;
    }
    pe.enable_on_exec = 1; //enable trace exec call
    pe.type = PERF_TYPE_HARDWARE;

    switch (method)
    {
    case _HF_DYNFILE_INSTR_COUNT:
        LL_LOG(INFO, PERF, "Using: PERF_COUNT_HW_INSTRUCTIONS for pid=%d\n", (int)run->pid);
        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        pe.inherit = 1; //trace child process
        break;
    case _HF_DYNFILE_BRANCH_COUNT:
        LL_LOG(INFO, PERF, "Using: PERF_COUNT_HW_BRANCH_INSTRUCTIONS for pid=%d\n", (int)run->pid);
        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        pe.inherit = 1; //trace child process
        break;
    case _HF_DYNFILE_BTS_TRACE:
        LL_LOG(INFO, PERF, "Using: (Intel BTS) type=%" PRIu32 " for pid=%d\n", perf_intel_bts_perf_type, (int)run->pid);
        pe.type = perf_intel_bts_perf_type;
        break;
    case _HF_DYNFILE_INS_TRACE:
        LL_LOG(INFO, PERF, "Using: (Intel PT) type=%" PRIu32 " for pid=%d\n", perf_intel_pt_perf_type, (int)run->pid);
        pe.type = perf_intel_pt_perf_type;
        pe.config = PT_RTIT_CTL_DISRETC | PT_RTIT_CTL_TSC_EN | PT_RTIT_CTL_MTC_EN;
        break;
    default:
        LL_LOG(DEBUG, PERF, "Unknown perf mode: '%d' for pid=%d\n", method, (int)run->pid);
        return false;
        break;
    }

    *perf_fd = sys_perf_event_open(&pe, run->pid, run->cpu_index, run->group_fd, PERF_FLAG_FD_CLOEXEC);
    if (*perf_fd == -1) {
        LL_LOG(DEBUG, PERF, "perf_event_open() failed, error code = %d\n", errno);
        return false;
    }

    run->global->feedback.dyn_file_method = method;

    if (method != _HF_DYNFILE_BTS_TRACE && method != _HF_DYNFILE_INS_TRACE) {
        return true;
    }
    
#if defined(PERF_ATTR_SIZE_VER5)


    run->arch_linux.perf_mmap_buf  = mmap(NULL, _HF_PERF_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *perf_fd, 0);
    if (run->arch_linux.perf_mmap_buf == MAP_FAILED) {
        run->arch_linux.perf_mmap_buf = NULL;
        LL_LOG(DEBUG, PERF, "mmap(mmapBuf) failed, sz=%zu, you can only mmap 2*pa_sz in perf_vt_event_fd for rdwt\n", (size_t)_HF_PERF_MAP_SIZE);
        close(*perf_fd);
        *perf_fd = -1;
        return false;
    }

    struct perf_event_mmap_page* pem = (struct perf_event_mmap_page*)run->arch_linux.perf_mmap_buf;
    pem->aux_offset                  = pem->data_offset + pem->data_size;
    ll_rmb();
    pem->aux_size                    = _HF_PERF_AUX_PAGE_NUM * pg_sz;

    run->arch_linux.perf_mmap_aux = mmap(NULL, pem->aux_size, PROT_READ, MAP_SHARED, *perf_fd, pem->aux_offset);
    if (run->arch_linux.perf_mmap_aux == MAP_FAILED) {
        munmap(run->arch_linux.perf_mmap_buf, _HF_PERF_MAP_SIZE);
        run->arch_linux.perf_mmap_buf = NULL;
        run->arch_linux.perf_mmap_aux = NULL;
        LL_LOG(DEBUG, PERF, "mmap(mmapAuxBuf) failed, try increasing the kernel.perf_event_mlock_kb sysctl (up to " \
            "even 300000000)\n", (size_t)_HF_PERF_AUX_PAGE_NUM * pg_sz);
        close(*perf_fd);
        *perf_fd = -1;
        return false;
    }
    LL_LOG(INFO, PERF, "Sucess open perf vt trace\n");
#else /* defined(PERF_ATTR_SIZE_VER5) */
    LL_LOG(INFO, PERF, "Your <linux/perf_event.h> includes are too old to support Intel PT/BTS\n");
#endif /* defined(PERF_ATTR_SIZE_VER5) */
    return true;
}

bool ll_arch_perf_open(struct run_t *run) {
    if (run->global->feedback.dyn_file_method == _HF_DYNFILE_NONE) {
        return true;
    }

    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INSTR_COUNT) {
        if (unlikely(!ll_arch_perf_create(run, _HF_DYNFILE_INSTR_COUNT, &run->arch_linux.cpu_branch_fd))) {
            LL_LOG(DEBUG, PERF, "Cannot set up perf for pid=%d (_HF_DYNFILE_INSTR_COUNT)\n", (int)run->pid);
            goto out;
        }
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BRANCH_COUNT) {
        if (unlikely(!ll_arch_perf_create(run, _HF_DYNFILE_BRANCH_COUNT, &run->arch_linux.cpu_instr_fd))) {
            LL_LOG(DEBUG, PERF, "Cannot set up perf for pid=%d (_HF_DYNFILE_BRANCH_COUNT)\n", (int)run->pid);
            goto out;
        }
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BTS_TRACE) {
        if (unlikely(!ll_arch_perf_create(run, _HF_DYNFILE_BTS_TRACE, &run->arch_linux.cpu_ipt_bts_fd))) {
            LL_LOG(DEBUG, PERF, "Cannot set up perf for pid=%d (_HF_DYNFILE_BTS_TRACE)\n", (int)run->pid);
            goto out;
        }
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INS_TRACE) {
        if (unlikely(!ll_arch_perf_create(run, _HF_DYNFILE_INS_TRACE, &run->arch_linux.cpu_ipt_bts_fd))) {
            LL_LOG(DEBUG, PERF, "Cannot set up perf for pid=%d (_HF_DYNFILE_INS_TRACE)\n", (int)run->pid);
            goto out;
        }
    }

    return true;

out:
    ll_arch_perf_close(run);
    return false;
}


bool ll_arch_perf_enable(struct run_t* run) {

    int ret;

    if (run->global->feedback.dyn_file_method == _HF_DYNFILE_NONE) {
        return true;
    }

    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INSTR_COUNT) {
        ret = sys_perf_event_enable(run->arch_linux.cpu_instr_fd);
        if (ret < 0) {
            return false;
        }
    } 
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BRANCH_COUNT) {
        ret = sys_perf_event_enable(run->arch_linux.cpu_branch_fd);
        if (ret < 0) {
            return false;
        }
    } 
    
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BTS_TRACE) {
        ret = sys_perf_event_enable(run->arch_linux.cpu_ipt_bts_fd);
        if (ret < 0) {
            return false;
        }
    } 
    
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INS_TRACE) {
        ret = sys_perf_event_enable(run->arch_linux.cpu_ipt_bts_fd);
        if (ret < 0) {
            return false;
        }
    }
    
    return true;
}

static void ll_arch_perf_mmap_reset(struct run_t *run) {

#if defined(PERF_ATTR_SIZE_VER5)

    ll_wmb();

    struct perf_event_mmap_page *pem = (struct perf_event_mmap_page *)run->arch_linux.perf_mmap_buf;
    ll_atomic64_set((int64_t *)&pem->data_head, 0);
    ll_atomic64_set((int64_t *)&pem->data_tail, 0);
    ll_atomic64_set((int64_t *)&pem->aux_head, 0);
    ll_atomic64_set((int64_t *)&pem->aux_tail, 0);
#endif /* defined(PERF_ATTR_SIZE_VER5) */
}

void ll_arch_perf_analyze(struct run_t *run) {
    uint64_t instr_count;
    uint64_t branch_count;
    if (run->global->feedback.dyn_file_method == _HF_DYNFILE_NONE) {
        return ;
    }
    
    if ((run->global->feedback.dyn_file_method & _HF_DYNFILE_INSTR_COUNT) && (run->arch_linux.cpu_instr_fd != -1)) {
        sys_perf_event_disable(run->arch_linux.cpu_instr_fd);
        if (ll_file_read_from_fd(run->arch_linux.cpu_instr_fd, (uint8_t *)&instr_count, sizeof(uint64_t)) != sizeof(uint64_t)) {
            LL_LOG(DEBUG, PERF, "perf instr_count read(perf_fd='%d') failed", (int)run->arch_linux.cpu_instr_fd);
        }
        sys_perf_event_enable(run->arch_linux.cpu_instr_fd);
    }

    if ((run->global->feedback.dyn_file_method & _HF_DYNFILE_BRANCH_COUNT) && (run->arch_linux.cpu_branch_fd != -1)) {
        sys_perf_event_disable(run->arch_linux.cpu_branch_fd);
        if (ll_file_read_from_fd(run->arch_linux.cpu_branch_fd, (uint8_t *)&branch_count, sizeof(uint64_t)) != sizeof(uint64_t)) {
            LL_LOG(DEBUG, PERF, "perf branch_count read(perf_fd='%d') failed", (int)run->arch_linux.cpu_branch_fd);
        }
        sys_perf_event_enable(run->arch_linux.cpu_branch_fd);
    }
    
    if ((run->global->feedback.dyn_file_method & _HF_DYNFILE_BTS_TRACE) && (run->arch_linux.cpu_ipt_bts_fd != -1)) {
        sys_perf_event_disable(run->arch_linux.cpu_ipt_bts_fd);
        ll_arch_pt_analyze(run);
        ll_arch_perf_mmap_reset(run);
        sys_perf_event_enable(run->arch_linux.cpu_ipt_bts_fd);
    }

    if ((run->global->feedback.dyn_file_method & _HF_DYNFILE_INS_TRACE) && (run->arch_linux.cpu_ipt_bts_fd != -1)) {
        sys_perf_event_disable(run->arch_linux.cpu_ipt_bts_fd);
        ll_arch_pt_analyze(run);
        ll_arch_perf_mmap_reset(run);
        sys_perf_event_enable(run->arch_linux.cpu_ipt_bts_fd);
    }

    run->hw_cnt.cpu_instr_cnt = instr_count;
    run->hw_cnt.cpu_branch_cnt = branch_count;

}


void ll_arch_perf_close(struct run_t *run) {
    if (run->global->feedback.dyn_file_method == _HF_DYNFILE_NONE) {
        return;
    }

    if (run->arch_linux.perf_mmap_aux != NULL) {
        munmap(run->arch_linux.perf_mmap_aux, _HF_PERF_AUX_PAGE_NUM * ll_mem_page_size());
        run->arch_linux.perf_mmap_aux = NULL;
    }
    if (run->arch_linux.perf_mmap_buf != NULL) {
        munmap(run->arch_linux.perf_mmap_buf, _HF_PERF_MAP_SIZE);
        run->arch_linux.perf_mmap_buf = NULL;
    }

    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INSTR_COUNT) {
        close(run->arch_linux.cpu_instr_fd);
        run->arch_linux.cpu_instr_fd = -1;
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BRANCH_COUNT) {
        close(run->arch_linux.cpu_branch_fd);
        run->arch_linux.cpu_branch_fd = -1;
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_BTS_TRACE) {
        close(run->arch_linux.cpu_ipt_bts_fd);
        run->arch_linux.cpu_ipt_bts_fd = -1;
    }
    if (run->global->feedback.dyn_file_method & _HF_DYNFILE_INS_TRACE) {
        ll_arch_pt_stop(run);
    }

}


bool ll_arch_perf_init(struct run_t *run) {
    
    if (unlikely(run == NULL)) {
        return false;
    }
    if (unlikely(run->global == NULL)) {
        run->global = ll_zmalloc("perf_global_set", sizeof(struct perf_t), 0);
        if (unlikely(run->global == NULL)) {
            LL_LOG(DEBUG, PERF, "ll_zmalloc is error, memory is not enough!\n");
            ll_errno = ENOMEM;
            return false;
        }
    }
    
    ll_arch_perf_run_set(run, -1, -1, -1);

    if (perf_intel_pt_perf_type != -1 || perf_intel_bts_perf_type != -1) {
        return true;
    }

    static char const intel_pt_path[]  = "/sys/bus/event_source/devices/intel_pt/type";
    static char const intel_bts_path[] = "/sys/bus/event_source/devices/intel_bts/type";

    if (ll_file_exists(intel_pt_path)) {
        uint8_t buf[256];
        ssize_t sz = ll_file_read_from_file(intel_pt_path, buf, sizeof(buf) - 1);
        if (sz > 0) {
            buf[sz]             = '\0';
            perf_intel_pt_perf_type = (int32_t)strtoul((char*)buf, NULL, 10);
            LL_LOG(INFO, PERF, "perf_intel_pt_perf_type = %" PRIu32 "\n", perf_intel_pt_perf_type);
        }
    }
    
    if (ll_file_exists(intel_pt_path)) {
        uint8_t buf[256];
        ssize_t sz = ll_file_read_from_file(intel_bts_path, buf, sizeof(buf) - 1);
        if (sz > 0) {
            buf[sz]             = '\0';
            perf_intel_bts_perf_type = (int32_t)strtoul((char*)buf, NULL, 10);
            LL_LOG(INFO, PERF, "perf_intel_bts_perf_type = %" PRIu32 "\n", perf_intel_bts_perf_type);
        }
    }

    __pt_cpu_init();

    return true;
}