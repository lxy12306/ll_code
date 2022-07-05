#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/syscall.h>


#include <stdio.h>
#include <stdbool.h>


#include <xed-interface.h>
#include <xed-print-info.h>

#include <ll_common.h>
#include <ll_errno.h>
#include <ll_log.h>
#include <ll_memory.h>
#include <ll_file.h>


#include "ll_pmem.h"
#include "../perf/util/intel_pt_decoder/intel_pt_insn_decoder.h"



#define _PROC_MEM_MAX_PATH LL_PATH_MAX

#define THIS_DEBUG 1


pid_t ll_pmem_ptrace_process(const char * full_path, const char *process_name) {
	pid_t pid = 0;
	int val = 0;
    struct user_regs_struct regs = {0};
    pid = fork();

	if (pid == -1) {
		LL_LOG(DEBUG, PROC, "ll_pmem_ptrace_process fork error");
        return -1;
    } else if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL); //子进程希望父进程追踪它
        if(execl(full_path , process_name, NULL) < 0) {
			LL_LOG(DEBUG, PROC, "ll_pmem_ptrace_process execl error");
        }
    } else {
        while (1) {
            wait(&val); //等待并记录execve
            if(WIFEXITED(val))
                return -1; //子进程正常退出
            ptrace(PTRACE_GETREGS, pid, 0, &regs);

            if(regs.orig_rax == SYS_write) {
				return pid;
            } else {
                ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
            }
        }
    }
	return -1;
}


struct ll_process_mem_maps* ll_pmem_get_proc_maps(pid_t pid) {

	FILE *f = NULL;
	ssize_t ret = 0;
	char buf[_PROC_MEM_MAX_PATH] = {0};
	char *buf_0 = NULL;
	char *buf_1 = NULL;
	size_t n;
	struct ll_pmem_info pmem_info = {0};
	int i = 0;

	struct ll_process_mem_maps *maps = ll_zmalloc("process_mem_maps", sizeof(struct ll_process_mem_maps), 0);

	if (unlikely(maps == NULL)) {
		ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory");
		return NULL;
	}
	
	snprintf(buf, _PROC_MEM_MAX_PATH, "/proc/%d/maps", pid);

	f = fopen(buf, "r");
	if (f == NULL) {
		ll_errno = -EBADF;
		LL_LOG(DEBUG, PROC, "Can not open file %s", buf);
		return NULL;
	}

	ret = getline(&buf_1, &n, f);
	sscanf(buf_1, "%lx-%lx %s %lx %x:%x %d\t\t%s\n", &(pmem_info.begin),
					&(pmem_info.end), pmem_info.auth,
					&(pmem_info.begin_offset), &pmem_info.dev_major,
					&pmem_info.dev_minor, &pmem_info.inode_index,
					pmem_info.path);
#if THIS_DEBUG
	printf("%lx-%lx %s %lx %x:%x %d\t\t%s\n", (pmem_info.begin),
					(pmem_info.end), pmem_info.auth,
					(pmem_info.begin_offset), pmem_info.dev_major,
					pmem_info.dev_minor, pmem_info.inode_index,
					pmem_info.path);
#endif
	maps->pid = pid;
	do {

		buf_0 = (char *)ll_zmalloc("", _PROC_MEM_MAX_PATH, 0);
		if (unlikely(buf_0 == NULL)) {
			ll_errno = -ENOMEM;
			LL_LOG(DEBUG, PROC, "Can not get memory for buf");
			goto _exit;
		}
		memcpy(buf_0, pmem_info.path, strlen(pmem_info.path) + 1);

		do {

			if (strcmp(buf_0, pmem_info.path) != 0) {
				break;
			}

			if (strcmp(pmem_info.auth, "r-xp") == 0) {
				if (maps->text[i].begin == NULL) {
					maps->text[i].begin = (void *)pmem_info.begin;
					maps->text[i].end = (void *)pmem_info.end;
					maps->text[i].len = (size_t)pmem_info.end - pmem_info.begin;
					maps->text[i].path = buf_0;
					maps->text[i].buf = NULL;
					++i;
				} else {
					maps->text[i].end = (void *)pmem_info.end;
					maps->text[i].len += (size_t)pmem_info.end - pmem_info.begin;
				}
			} else if (strcmp(pmem_info.auth, "rw-p") == 0) {
				if (maps->data[i].begin == NULL) {
					maps->data[i].begin = (void *)pmem_info.begin;
					maps->data[i].end = (void *)pmem_info.end;
					maps->data[i].len = (size_t)pmem_info.end - pmem_info.begin;
					maps->data[i].path = buf_0;
					maps->data[i].buf = NULL;
				} else {
					maps->data[i].end = (void *)pmem_info.end;
					maps->data[i].len += (size_t)pmem_info.end - pmem_info.begin;
				}
			}
			ret = getline(&buf_1, &n, f);
			sscanf(buf_1, "%lx-%lx %s %lx %x:%x %d\t\t%s\n", &(pmem_info.begin),
					&(pmem_info.end), pmem_info.auth,
					&(pmem_info.begin_offset), &pmem_info.dev_major,
					&pmem_info.dev_minor, &pmem_info.inode_index,
					pmem_info.path);
#if THIS_DEBUG
			printf("%lx-%lx %s %lx %x:%x %d\t\t%s\n", (pmem_info.begin),
					(pmem_info.end), pmem_info.auth,
					(pmem_info.begin_offset), pmem_info.dev_major,
					pmem_info.dev_minor, pmem_info.inode_index,
					pmem_info.path);
#endif
			if (ret == -1) {
				goto _exit;
			}
			//free(buf_1);
			
		} while (true);


	} while(true);
_exit:
	free(buf_1);
	return maps;
}

void ll_pmem_free_proc_maps(struct ll_process_mem_maps* maps) {
	
	for (int i = 0; i < LL_PMEM_MAPS_MAX; ++i) {
		
		if (maps->text[i].path != NULL) {
			ll_free(maps->text[i].path);
			maps->text[i].path = NULL;
			maps->data[i].path = NULL;
		}
		
		if (maps->text[i].buf != NULL) {
			ll_free(maps->text[i].buf);
			maps->text[i].buf = NULL;
		}

		if (maps->data[i].buf != NULL) {
			ll_free(maps->data[i].buf);
			maps->data[i].buf = NULL;
		}
	}

	ll_free(maps);
}


int __ll_pmem_default_disassembly_callback_fn (
    xed_uint64_t  address,
    char*         symbol_buffer,
    xed_uint32_t  buffer_length,
    xed_uint64_t* offset,
    void*         context) {
    return 0;
}


xed_print_info_t* _ll_pmem_create_print_info() {

    xed_print_info_t * pi = ll_zmalloc("xed_print_info", sizeof(xed_print_info_t), 0);
    if (pi == NULL) {
    	ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory for xed_print_info_t");
		return NULL;
    }

    /*
        ...
        set options code add
    */
    pi->format_options.lowercase_hex = true;
    xed_format_set_options(pi->format_options);
    xed_init_print_info(pi);

    pi->buf = NULL;
    pi->blen = LL_ALIGN(25,LL_CACHE_LINE_SIZE);
    pi->disassembly_callback = __ll_pmem_default_disassembly_callback_fn;
    pi->context = NULL;

    return pi;
}

int _ll_pmem_format_context(const xed_decoded_inst_t *xedd, xed_print_info_t * pi) {
    xed_bool_t ret;

    if (pi->buf == NULL) {
        pi->buf = ll_zmalloc("xed_print_info_buf", LL_ALIGN(25,LL_CACHE_LINE_SIZE), 0);
        if (pi->buf == NULL) {
    	    ll_errno = -ENOMEM;
		    LL_LOG(DEBUG, PROC, "Can not get memory for xed_print_info_t.buf");
            ll_free(pi);
		    return -1;
        }
    } 

    ret = xed_format_context(XED_SYNTAX_INTEL, xedd, pi->buf, pi->blen,
        pi->runtime_address, pi->context, pi->disassembly_callback);
    if (ret == false) {
        return -1;
    } else {
        pi->p = xedd; 
        if (strcmp(pi->buf, "nop edx, edi") == 0) {
            memcpy(pi->buf,"endbr64", sizeof("endbr64") + 1);
        }
        return 0;
    }

	return -1;
}


char* _ll_pmem_getdata(pid_t child, uint64_t addr, size_t len) {

	char *buf = NULL;
    int count = 0;
    int max_count = len / sizeof(long);
	char * buf_temp;

	buf = ll_zmalloc("", len, 0);
    if (buf == NULL) {
    	ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory for buf");
		return NULL;
    }

    union {
        long raw_data;
        char str_data[sizeof(long)];
    } converter;
    
    if (len % sizeof(long) != 0) {
        max_count++;
    }

	buf_temp = buf;
    while (count < max_count) {
        if (count == max_count - 1) {
            memset(&converter, 0, sizeof(long));
        }
        converter.raw_data = ptrace(PTRACE_PEEKDATA, child, addr,  NULL);

        memcpy(buf_temp, converter.str_data, sizeof(long));
        addr += sizeof(long);
        buf_temp += sizeof(long);
        ++count;
    }
    
	return buf;
}


struct insn* ll_pmem_analyze_function(struct ll_process_mem_maps * pmap, uint64_t addr, uint64_t *next_addr) {
	char *buf = NULL;
	char *xed_buf = NULL;
	size_t max_len = 0;
	char *buf_bound = NULL;
	int i;

	struct insn *prev, *head, *next;

	xed_print_info_t *pi = NULL;
	xed_state_t xed_state;
	xed_error_enum_t xed_error;
    xed_decoded_inst_t xedd;

	for (i = 0; i < LL_PMEM_MAPS_MAX; ++i) {
		uint64_t begin, end;
		begin = (uint64_t)pmap->text[i].begin;
		end = (uint64_t)pmap->text[i].end;

		if (addr >= begin && addr < end) {
			if (unlikely(pmap->text[i].buf == NULL)) {
				pmap->text[i].buf = _ll_pmem_getdata(pmap->pid, (uint64_t)pmap->text[i].begin, pmap->text[i].len);
			}
			buf = (char *)((uint64_t)pmap->text[i].buf + (addr - begin));
			max_len = pmap->text[i].len - (addr - begin);
			break;
		}
	}

	if (unlikely(buf == NULL)) {
		ll_errno = -EFAULT;// bad address
		LL_LOG(DEBUG, PROC, "bad address for input addr");
        *next_addr = (uint64_t)-1;
		return NULL;
	}


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



    // The state of the machine -- required for decoding
	pi = _ll_pmem_create_print_info();

    xed_state_init(&xed_state, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b, XED_ADDRESS_WIDTH_64b);
    xed_tables_init();
    xed_decoded_inst_zero_set_mode(&xedd, &xed_state);

	xed_buf = buf;
	buf_bound = (char *)((uint64_t)xed_buf + max_len);

    for(; xed_buf < buf_bound;) {

        xed_decoded_inst_zero_keep_mode(&xedd);
        xed_error = xed_decode(&xedd, XED_STATIC_CAST(const xed_uint8_t*,xed_buf),15);
        if (xed_error != XED_ERROR_NONE) {
			LL_LOG(INFO, PROC, "xed_decode error : %s\n", xed_error_enum_t2str(xed_error));
            break;
        }

        if (xedd._decoded_length == 2) { //A similar 00 00 instruction appears,This means that the function segment ends
            unsigned short *tmp = (unsigned short *)xedd._byte_array._enc;
            if (*tmp == 0) {
                while (xed_buf < buf_bound) {
                    if(*xed_buf == 0) {
                        ++xed_buf;
                    } else {
                        break;
                    }
                }
                break;
            }
        }

        prev->next = next; //Link list
        pi->runtime_address = addr + (uint64_t)(xed_buf - buf);


        next->pos_opcode = xedd._operands.pos_nominal_opcode;
        next->pos_modrm = xedd._operands.pos_modrm;
        next->pos_sib = xedd._operands.pos_sib;
        next->pos_disp = xedd._operands.pos_disp;
        next->pos_imm = xedd._operands.pos_imm;
		next->addr = pi->runtime_address;

        next->length = xedd._decoded_length;
        next->avx = xed_decoded_inst_vector_length_bits(&xedd);

        next->immediate.nbytes = xed_decoded_inst_get_immediate_width(&xedd);
        if (next->immediate.nbytes != 0) {
            next->immediate.value = xed_decoded_inst_get_signed_immediate(&xedd);
        }

		next->displacement.nbytes = xed_decoded_inst_get_branch_displacement_width(&xedd);
        if (next->displacement.nbytes != 0) {
            next->displacement.value = xed_decoded_inst_get_branch_displacement(&xedd);
        }

        for(i = 0; i < next->length; ++i) {
            next->buf[i] = *(xed_buf++);
        }
        next->buf[i] = 0;

        _ll_pmem_format_context(&xedd, pi);
        next->pinfo = pi->buf;
#if THIS_DEBUG
        printf("%-#10lx: %s\n", pi->runtime_address, pi->buf);
#endif
        pi->buf = NULL;

        next = ll_zmalloc("", sizeof(struct insn), 0);
        if (next == NULL) {
    	    ll_errno = -ENOMEM;
		    LL_LOG(DEBUG, PROC, "Can not get memory for insn(next)");
            *next_addr = (uint64_t)xed_buf;
		    return head;
        }
        prev = prev->next;
    }

    *next_addr = addr + (uint64_t)(xed_buf - buf);
    return head;
}



struct insn * ll_pmem_analyze_instruction(const unsigned char *buf, size_t len, uintptr_t addr, int x86_64)  {
    static xed_print_info_t *pi = NULL;
	static xed_state_t xed_state;
	static xed_error_enum_t xed_error;
    static xed_decoded_inst_t xedd;
    int i;
    struct insn * insn = NULL;

    if (pi == NULL) {
        if (x86_64) {
            xed_state_init(&xed_state, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b, XED_ADDRESS_WIDTH_64b);
        } else {
            xed_state_init(&xed_state, XED_MACHINE_MODE_LONG_COMPAT_32, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b);
        }
        xed_tables_init();
        xed_decoded_inst_zero_set_mode(&xedd, &xed_state);
    	pi = _ll_pmem_create_print_info();
    }

    insn = ll_zmalloc("", sizeof(struct insn), 0);
    if (unlikely(insn == NULL)) {
    	ll_errno = -ENOMEM;
		LL_LOG(DEBUG, PROC, "Can not get memory for insn(insn)");
		return NULL;
    }

    xed_decoded_inst_zero_keep_mode(&xedd);
    xed_error = xed_decode(&xedd, XED_STATIC_CAST(const xed_uint8_t*,buf),len);
    if (xed_error != XED_ERROR_NONE) {
		LL_LOG(INFO, PROC, "xed_decode error : %s\n", xed_error_enum_t2str(xed_error));
        ll_pmem_free_insn_list(insn);
        return NULL;
    }

    pi->runtime_address = addr;
    insn->pos_opcode = xedd._operands.pos_nominal_opcode;
    insn->pos_modrm = xedd._operands.pos_modrm;
    insn->pos_sib = xedd._operands.pos_sib;
    insn->pos_disp = xedd._operands.pos_disp;
    insn->pos_imm = xedd._operands.pos_imm;
	insn->addr = pi->runtime_address;

    insn->length = xedd._decoded_length;
    insn->avx = xed_decoded_inst_vector_length_bits(&xedd);

    insn->immediate.nbytes = xed_decoded_inst_get_immediate_width(&xedd);
    if (insn->immediate.nbytes != 0) {
        insn->immediate.value = xed_decoded_inst_get_signed_immediate(&xedd);
    }

	insn->displacement.nbytes = xed_decoded_inst_get_branch_displacement_width(&xedd);
    if (insn->displacement.nbytes != 0) {
        insn->displacement.value = xed_decoded_inst_get_branch_displacement(&xedd);
    }

    for(i = 0; i < insn->length; ++i) {
        insn->buf[i] = *(buf++);
    }
    insn->buf[i] = 0;

    _ll_pmem_format_context(&xedd, pi);
    insn->pinfo = pi->buf;
    pi->buf = NULL;

    return insn;
}


void ll_pmem_free_insn(struct insn *insn) {

	if (insn->pinfo != NULL) {
		ll_free(insn->pinfo);
	}

	ll_free(insn);
}


void ll_pmem_free_insn_list(struct insn *head) {
	struct insn *prev = head;
	struct insn *next = head->next;

	while (next) {
		if (prev->pinfo != NULL) {
			ll_free(prev->pinfo);
		}
		ll_free(prev);
		prev = next;
		next = next->next;
	}
}

void ll_pmem_printf_insn_list(FILE *f, struct insn *head) {
    struct insn *p = head->next;

    while (p) {
        //fprintf(f, "%-#10lx: %16s : %s\n", p->addr, p->buf, p->pinfo);
        fprintf(f, "%-#10lx: ", p->addr);
        for (int i = 0; i < p->length; ++i) {
            fprintf(f, "%02x ", p->buf[i]);
        }
        fprintf(f, ": %s\t\t", p->pinfo);
        
        if (p->pos_modrm != 0) {
            fprintf(f, "|opcode:");
            for (int i = p->pos_opcode; i < p->pos_modrm; ++i) {
                fprintf(f, " %02x", p->buf[i]);
            }
            fprintf(f, "|\t\t");
        } else if (p->pos_sib != 0) {
            fprintf(f, "|opcode:");
            for (int i = p->pos_opcode; i < p->pos_sib; ++i) {
                fprintf(f, " %02x", p->buf[i]);
            }
            fprintf(f, "|\t\t");
        } else if (p->pos_disp != 0){
            fprintf(f, "|opcode:");
            for (int i = p->pos_opcode; i < p->pos_disp; ++i) {
                fprintf(f, " %02x", p->buf[i]);
            }
            fprintf(f, "|\t\t");
        } else if (p->pos_imm != 0){
            fprintf(f, "|opcode:");
            for (int i = p->pos_opcode; i < p->pos_imm; ++i) {
                fprintf(f, " %02x", p->buf[i]);
            }
            fprintf(f, "|\t\t");
        } else {
            fprintf(f, "|opcode:");
           for (int i = p->pos_opcode; i < p->length; ++i) {
                fprintf(f, " %02x", p->buf[i]);
            }
            fprintf(f, "|\t\t");
        }

        if (p->immediate.nbytes != 0) {
            fprintf(f, "|imm: %d |\t\t", p->immediate.value);
        }

		if (p->displacement.nbytes != 0) {
            fprintf(f, "|disp: %x |\t\t", p->displacement.value);
        }

        fprintf(f, "\n");
        p = p->next;
    }
}