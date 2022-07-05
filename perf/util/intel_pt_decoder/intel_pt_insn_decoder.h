#ifndef INCLUDE__INTEL_PT_INSN_DECODER_H__
#define INCLUDE__INTEL_PT_INSN_DECODER_H__

#include <stddef.h>
#include <stdint.h>

#define INTEL_PT_INSN_DESC_MAX		32
#define INTEL_PT_INSN_BUF_SZ		16

typedef unsigned char insn_byte_t;
typedef signed int insn_value_t;

enum intel_pt_insn_op {
	INTEL_PT_OP_UNKOWN,
	INTEL_PT_OP_OTHER,
	INTEL_PT_OP_CALL,
	INTEL_PT_OP_RET,
	INTEL_PT_OP_JCC,
	INTEL_PT_OP_JMP,
	INTEL_PT_OP_LOOP,
	INTEL_PT_OP_IRET,
	INTEL_PT_OP_INT,
	INTEL_PT_OP_SYSCALL,
	INTEL_PT_OP_SYSRET,
	INTEL_PT_OP_VMENTRY,
};

enum intel_pt_insn_branch {
	INTEL_PT_BR_NO_BRANCH,
	INTEL_PT_BR_INDIRECT,
	INTEL_PT_BR_CONDITIONAL,
	INTEL_PT_BR_UNCONDITIONAL,
};


struct insn_field {
	union {
		insn_value_t value;
		insn_byte_t bytes[4];
	};
	/* !0 if we've run insn_get_xxx() for this field */
	unsigned char got;
	unsigned char nbytes;
};

struct insn {
	int				length;
	struct insn *next;
    struct insn *jmp;
	char* pinfo;
	uintptr_t addr;
	union 
	{
		struct insn_field immediate;
		struct insn_field immediate1;
	};
	struct insn_field immediate2;
	struct insn_field displacement;
	unsigned char			buf[INTEL_PT_INSN_BUF_SZ];
    uint8_t pos_opcode;
    uint8_t pos_modrm;
    uint8_t pos_sib;
    uint8_t pos_disp;
	uint8_t pos_imm;
	uint8_t avx;
};

struct intel_pt_insn {
	enum intel_pt_insn_op op;
	enum intel_pt_insn_branch branch;
	int length;
	int32_t rel;
	unsigned char *buf;
	struct insn *insn;
};

int intel_pt_get_insn(const unsigned char *buf, size_t len, int x86_64,
		      struct intel_pt_insn *intel_pt_insn);

const char *intel_pt_insn_name(enum intel_pt_insn_op op);

int intel_pt_insn_desc(const struct intel_pt_insn *intel_pt_insn, char *buf,
		       size_t buf_len);
#if 0
int intel_pt_insn_type(enum intel_pt_insn_op op);
#endif

#endif
