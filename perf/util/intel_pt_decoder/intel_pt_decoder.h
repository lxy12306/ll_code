#ifndef _INTEL_PT_DECODER_H_
#define _INTEL_PT_DECODER_H_



#include "intel_pt_pkt_decoder.h"
#include "intel_pt_log.h"

struct intel_pt_decoder {


	uint64_t pos; //aux pos
	const unsigned char *buf; //aux buf
	size_t len;//aux size


	enum intel_pt_pkt_ctx prev_pkt_ctx;
	int last_packet_type;
	enum intel_pt_pkt_ctx pkt_ctx;

	struct intel_pt_pkt packet;
	struct intel_pt_pkt tnt;
	int pkt_step;
	int pkt_len;


	uint64_t last_ip; //last_ip for ip compress
	uint64_t ip;//now ip


	bool have_last_ip;
};


struct intel_pt_buffer {
	const unsigned char *buf; //buffer地址
	size_t len; //buffer长度
	bool consecutive; //是否连续
	uint64_t ref_timestamp; //引用该buf的时间戳
	uint64_t trace_nr;//
};

struct intel_pt_params {
	const char* name;
	void *data;
	size_t len;
};

//初始化新的pt_decoder
struct intel_pt_decoder *intel_pt_decoder_new(struct intel_pt_params *params);

//获取下一个pt有效数据包
int intel_pt_get_next_packet(struct intel_pt_decoder *decoder);

#endif //_INTEL_PT_DECODER_H_