#include <stdbool.h>
#include <stdio.h>

#include <ll_memory.h>

#include "intel_pt_decoder.h"
#include "intel_pt_log.h"


static uint64_t intel_pt_calc_ip(const struct intel_pt_pkt *packet,
				 uint64_t last_ip)
{
	uint64_t ip;

	switch (packet->count) {
	case 1:
		ip = (last_ip & (uint64_t)0xffffffffffff0000ULL) |
		     packet->payload;
		break;
	case 2:
		ip = (last_ip & (uint64_t)0xffffffff00000000ULL) |
		     packet->payload;
		break;
	case 3:
		ip = packet->payload;
		/* Sign-extend 6-byte ip */
		if (ip & (uint64_t)0x800000000000ULL)
			ip |= (uint64_t)0xffff000000000000ULL;
		break;
	case 4:
		ip = (last_ip & (uint64_t)0xffff000000000000ULL) |
		     packet->payload;
		break;
	case 6:
		ip = packet->payload;
		break;
	default:
		return 0;
	}

	return ip;
}

static inline void intel_pt_set_last_ip(struct intel_pt_decoder *decoder)
{
	decoder->last_ip = intel_pt_calc_ip(&decoder->packet, decoder->last_ip);
	decoder->have_last_ip = true;
}

static inline void intel_pt_set_ip(struct intel_pt_decoder *decoder)
{
	intel_pt_set_last_ip(decoder);
	decoder->ip = decoder->last_ip;
}


static int intel_pt_bad_packet(struct intel_pt_decoder *decoder)
{
	return -EBADMSG;
}


int intel_pt_get_next_packet(struct intel_pt_decoder *decoder)
{
	int ret;

	decoder->last_packet_type = decoder->packet.type;

	do {
		decoder->pos += decoder->pkt_step;
		decoder->buf += decoder->pkt_step;
		decoder->len -= decoder->pkt_step;

		if (unlikely(decoder->len == 0)) {
			return ret;
		}

		decoder->prev_pkt_ctx = decoder->pkt_ctx;
		ret = intel_pt_get_packet(decoder->buf, decoder->len,
					  &decoder->packet, &decoder->pkt_ctx);
		if (unlikely(ret <= 0))
			return intel_pt_bad_packet(decoder);

		decoder->pkt_len = ret;
		decoder->pkt_step = ret;
		//intel_pt_decoder_log_packet(decoder);
	} while (decoder->packet.type == INTEL_PT_PAD);

	return 0;
}



struct intel_pt_decoder *intel_pt_decoder_new(struct intel_pt_params *params)
{
	struct intel_pt_decoder *decoder;
	FILE *fp = NULL;

	decoder = ll_zmalloc("intel_pt_decoder", sizeof(struct intel_pt_decoder), 0);
	if (!decoder)
		return NULL;
    
	decoder->pos = 0;
	decoder->buf = params->data;
	decoder->len = params->len;

	intel_pt_log_disable();
	if ((fp = intel_pt_log_fp()) != NULL) {
		fclose(fp);
	}
	intel_pt_log_enable();

    intel_pt_log_set_name(params->name);

	

	return decoder;
}

