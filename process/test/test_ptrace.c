

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/syscall.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <ll_log.h>
#include <ll_errno.h>
#include <ll_print_c.h>
#include <ll_memory.h>

#include <ll_pmem.h>

#include "../../perf/util/intel_pt_decoder/intel_pt_insn_decoder.h"
#include <xed-interface.h>
#include <xed-print-info.h>

#define PATH "/media/lxy/SSD_1T/BaiduNetdiskWorkspace/ubantu/code/process/test/hello"
#define PATH2 "/home/lxy/ubantu_ssd/code/process/test/hello"

#define PRINT_REG(reg, regs) printf(#reg"= <%lld>\n", regs->reg)

void print_regs(struct user_regs_struct * regs) {
    PRINT_REG(orig_rax, regs); //存放系统调用id
    PRINT_REG(rbx, regs);
    PRINT_REG(rcx, regs);
    PRINT_REG(rdx, regs);
    PRINT_REG(rsi, regs);
}

#define LL_DECODE_STOP_LIMIT 5


#if 0
struct insn* ll_pmem_analyze_function(pid_t child, uint64_t addr, size_t max_len, uint64_t *next_addr) {
    char *buf = ll_zmalloc("", max_len, 0);
    getdata(child, addr, max_len, buf);
    //ll_print_vecvec_dw(buf, len / sizeof(unsigned) / 8, 8);

    struct insn *prev, *head, *next;
    head = prev = ll_zmalloc("", sizeof(struct insn), 0);
    if (head == NULL) {
    	ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory for insn(head)");
        *next_addr = addr;
		return NULL;
    }

    next = ll_zmalloc("", sizeof(struct insn), 0);
    if (next == NULL) {
    	ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory for insn(next)");
        *next_addr = addr;
		return NULL;
    }

    xed_print_info_t *pi = ll_create_print_info();
    xed_state_t xed_state;

    // The state of the machine -- required for decoding

    xed_state_init(&xed_state, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b, XED_ADDRESS_WIDTH_64b);
    xed_tables_init();
#if 0
    for(bytes = 15;bytes<=100;bytes++) {
        xed_error_enum_t xed_error;
        xed_decoded_inst_t xedd;
        xed_decoded_inst_zero(&xedd);
        xed_decoded_inst_set_mode(&xedd, mmode, addr_width);
        xed_error = xed_decode(&xedd, 
                               XED_STATIC_CAST(const xed_uint8_t*,itext),
                               bytes);
        ll_format_context(&xedd, pi);
        printf("%d %s\n",(int)bytes, xed_error_enum_t2str(xed_error));
    }
#endif
    xed_error_enum_t xed_error;
    xed_decoded_inst_t xedd;
    //xed_bool_t end; //panduan yige hanshu jieshu de biaozhi
    //xed_bool_t xr;
    xed_decoded_inst_zero_set_mode(&xedd, &xed_state);

    char *xed_buf = buf;

    for(; (size_t)(xed_buf - buf) < max_len;) {


        xed_decoded_inst_zero_keep_mode(&xedd);
        xed_error = xed_decode(&xedd, 
                XED_STATIC_CAST(const xed_uint8_t*,xed_buf),
                15);
        if (xed_error != XED_ERROR_NONE) {
            printf("%s\n", xed_error_enum_t2str(xed_error));
            break;
        }

        if (xedd._decoded_length == 2) {
            unsigned short *tmp = (unsigned short *)xedd._byte_array._enc;
            if (*tmp == 0) {
                while ((size_t)(xed_buf - buf) < max_len) {
                    if(*xed_buf == 0) {
                        ++xed_buf;
                    } else {
                        break;
                    }
                }
                break;
            }
        }

        prev->next = next;
        pi->runtime_address = addr + (uint64_t)(xed_buf - buf);
        next->addr = pi->runtime_address;
        next->pos_opcode = xedd._operands.pos_nominal_opcode;
        next->pos_modrm = xedd._operands.pos_modrm;
        next->pos_sib = xedd._operands.pos_sib;
        next->pos_disp = xedd._operands.pos_disp;

        next->pos_imm = xedd._operands.pos_imm;
        next->length = xedd._decoded_length;

        next->avx = xed_decoded_inst_vector_length_bits(&xedd);

        next->immediate.nbytes = xed_decoded_inst_get_immediate_width(&xedd);
        if (next->immediate.nbytes != 0) {
            next->immediate.value = xed_decoded_inst_get_signed_immediate(&xedd);
        }
        int i = 0;
        for(; i < next->length; ++i) {
            next->buf[i] = xed_buf[i];
        }
        next->buf[i] = 0;
        ll_format_context(&xedd, pi);
        next->pinfo = pi->buf;
        printf("%-#10lx: %s\n", pi->runtime_address, pi->buf);
        pi->buf = NULL;
        xed_buf += xedd._decoded_length;

        next = ll_zmalloc("", sizeof(struct insn), 0);
        if (next == NULL) {
    	    ll_errno = -ENOMEM;
		    LL_LOG(DEBUG, PROC, "Can not get memory for insn(next)");
            *next_addr = (uint64_t)xed_buf;
		    return head;
        }
        prev = prev->next;
    }

    ll_free(buf);
    *next_addr =addr +  (uint64_t)(xed_buf - buf);
    return head;
}

#endif



int main() {
    uint64_t next;
    pid_t pid = ll_pmem_ptrace_process(PATH,"hello");
    struct user_regs_struct regs = {0};
    struct ll_process_mem_maps *maps = NULL;

    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    print_regs(&regs);
    maps = ll_pmem_get_proc_maps(pid);
    struct insn *p = ll_pmem_analyze_function(maps, (uint64_t)maps->text[0].begin, &next);
    struct insn *p2 = ll_pmem_analyze_function(maps, next, &next);
    ll_pmem_printf_insn_list(stdout, p);
    ll_pmem_printf_insn_list(stdout, p2);

    ll_pmem_free_insn_list(p);
    ll_pmem_free_insn_list(p2);

    ll_pmem_free_proc_maps(maps);

    ptrace(PTRACE_CONT, pid, NULL, NULL);
#if 0
    if (pid == -1) {
        perror("fork error");
        return -1;
    } else if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL); //子进程希望夫进程追踪它
        if(execl(PATH2 , "hello", NULL) < 0) {
            printf("execl error\n");
        }
    } else {
        while (1) {
            wait(&val); //等待并记录execve
            if(WIFEXITED(val))
                return 0; //子进程正常退出了
        
            ptrace(PTRACE_GETREGS, pid, 0, &regs);

            if (start == 1) {
                ins = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
                if (regs.rip >= (unsigned long long)maps->text.begin && regs.rip <= (unsigned long long)maps->text.end) {
                    printf("EIP: %llx  Instruction executed: %lx\n", regs.rip, ins);
                }
                ptrace(PTRACE_CONT, pid, NULL, NULL);
                continue;
            }

            if(regs.orig_rax == SYS_write) {
                start = 1;
                print_regs(&regs);
                maps = ll_pmem_get_proc_maps(pid);
                printf("%p, %ld\n", maps->text.begin, maps->text.len);
                print_memory(pid, (uint64_t)maps->text.begin, maps->text.len);
                ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
            } else {
                ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
            }
        }
    }
#endif
    return 0;

}
