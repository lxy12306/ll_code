#include <stdio.h>
#include <string.h>

#include <ll_atomic.h>
#include <ll_log.h>

#include "perf.h"

#include "./util/intel_pt_decoder/intel_pt_decoder.h"

#define PCV_UNKNOWN ((uint16_t)-1)
#define PCV_INTEL   ((uint16_t)1)

#define PT_DECODER_NAME "intel_pt"

void ll_arch_pt_analyze(struct run_t* run) {
    //pt's perf pem data_head = data_tail = 0
    const char *decoder_name = PT_DECODER_NAME;
    struct perf_event_mmap_page* pem = (struct perf_event_mmap_page*)run->arch_linux.perf_mmap_buf;

    uint64_t aux_tail = (uint64_t)ll_atomic64_read((volatile int64_t *)&pem->aux_tail);
    uint64_t aux_head = (uint64_t)ll_atomic64_read((volatile int64_t *)&pem->aux_head);

    ll_rmb();

    struct intel_pt_params params = {0};

    params.data = (void *)LL_PTR_ADD(run->arch_linux.perf_mmap_aux, aux_tail);
    params.len = aux_head - aux_tail;
    params.name = decoder_name;
    
    struct intel_pt_decoder *decoder = intel_pt_decoder_new(&params);
    int ret;
    while (true) {
        ret = intel_pt_get_next_packet(decoder);
        if (ret < 0) {
            break;
        }
        __intel_pt_log_packet(&decoder->packet, decoder->pkt_len, decoder->pos, decoder->buf);
    }

    ll_atomic64_set((volatile int64_t *)&pem->aux_tail, aux_tail);
    ll_wmb();
}

void ll_arch_pt_stop(struct run_t* run) {
    close(run->arch_linux.cpu_ipt_bts_fd);
    run->arch_linux.cpu_ipt_bts_fd = -1;
    intel_pt_log_close();
}

struct pt_cpu {
    uint16_t vendor;
    uint16_t family;
    uint8_t model;
    uint8_t stepping;
};


struct pt_cpu __pt_cpu[] = {
    {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    }, {
    .vendor   = PCV_UNKNOWN,
    .family   = 0,
    .model    = 0,
    .stepping = 0,
    },
};

void
__pt_cpu_init(void) {
    int i = 0;
    FILE* f = fopen("/proc/cpuinfo", "rb\n");
    if (unlikely(!f)) {
        LL_LOG(DEBUG, PERF, "Couldn't open '/proc/cpuinfo'\n");
        return;
    }
    for (; i < sizeof(__pt_cpu) / sizeof(struct pt_cpu);) {
        char k[1024], t[1024], v[1024];
        int  ret = fscanf(f, "%1023[^\t]%1023[\t]: %1023[^\n]\n", k, t, v);
        if (ret == EOF) {
            break;
        }
        if (ret != 3) {
            break;
        }
        if (strcmp(k, "vendor_id") == 0) {
            if (strcmp(v, "GenuineIntel") == 0) {
                __pt_cpu[i].vendor = PCV_INTEL;
                LL_LOG(DEBUG, PERF, "IntelPT vendor: Intel\n");
            } else {
                __pt_cpu[i].vendor = PCV_UNKNOWN;
                LL_LOG(DEBUG, PERF, "Current processor is not Intel, IntelPT will not work\n");
            }
        }
        if (strcmp(k, "cpu family") == 0) {
            __pt_cpu[i].family = atoi(v);
            LL_LOG(DEBUG, PERF, "IntelPT family: %" PRIu16 "\n", __pt_cpu[i].family);
        }
        if (strcmp(k, "model") == 0) {
            __pt_cpu[i].model = atoi(v);
            LL_LOG(DEBUG, PERF, "IntelPT model: %" PRIu8 "\n", __pt_cpu[i].model);
        }
        if (strcmp(k, "stepping") == 0) {
            __pt_cpu[i].stepping = atoi(v);
            LL_LOG(DEBUG, PERF, "IntelPT stepping: %" PRIu8 "\n", __pt_cpu[i].stepping);
            ++i;
        }
    }
    fclose(f);
}
