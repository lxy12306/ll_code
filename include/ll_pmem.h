
#ifndef _LL_P_MEM_H_
#define _LL_P_MEM_H_

#include <sys/types.h>
#include <sys/file.h>

#define LL_PMEM_MAPS_MAX 8

struct insn;

struct ll_pmem_info {
	unsigned long begin;
	unsigned long end;
	unsigned long begin_offset;
	uint32_t dev_major;
	uint32_t dev_minor;
	uint32_t inode_index;

	char auth[5];
	char path[256];
};


struct ll_pmem_va {
	void *begin;
	void *end;
	char *buf;
	size_t len;
	char *path;
};



struct ll_process_mem_maps {
	pid_t pid;
	struct ll_pmem_va text[LL_PMEM_MAPS_MAX];
	struct ll_pmem_va data[LL_PMEM_MAPS_MAX];
};

/**
 * Start process using debug mode
 *
 * @param full_path
 * 	the process full path
 * 
 * @param process_name
 * 	the process name , just like hello
 * 
 * @return
 * 	the process id, pid
 */
pid_t ll_pmem_ptrace_process(const char * full_path, const char *process_name);

struct ll_process_mem_maps* ll_pmem_get_proc_maps(pid_t pid);

void ll_pmem_free_proc_maps(struct ll_process_mem_maps* maps);

/**
 * Analyze binary code
 *
 * @param pmap [in]
 * 	the process's pmap, what you want to analyze
 * 
 * @param addr [in]
 * 	the process's code virtual addr you want to analyze 
 * 
 * @param addr [out]
 * 	the next valid code segment addr just next the addr
 * 
 * @return
 * 	the list of struct insn
 */
struct insn* ll_pmem_analyze_function(struct ll_process_mem_maps * pmap, uint64_t addr, uint64_t *next_addr);


/**
 * Analyze binary code
 *
 * @param pmap [in]
 * 	the process's pmap, what you want to analyze
 * 
 * @param addr [in]
 * 	the process's code virtual addr you want to analyze 
 * 
 * @param addr [out]
 * 	the next valid code segment addr just next the addr
 * 
 * @return
 * 	success or not, 0 means success
 */
struct insn* ll_pmem_analyze_instruction(const unsigned char *buf, size_t len, uintptr_t addr, int x86_64);

/**
 * print insn some info
 *
 * @param f [in]
 * 	out put file
 * 
 * @param head [in]
 * 	the list head of insn
 */
void ll_pmem_printf_insn_list(FILE *f, struct insn *head);

/**
 * free insn 
 *
 * @param insn [in]
 * 	the insn you want to free
 */
void ll_pmem_free_insn(struct insn *insn);

/**
 * free insn list
 *
 * @param head [in]
 * 	the list of insn you want to free
 */
void ll_pmem_free_insn_list(struct insn *head);
	
#endif //_LL_P_MEM_H_

