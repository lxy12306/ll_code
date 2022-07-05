#ifndef _LL_RING_CORE_H
#define _LL_RING_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#include <ll_common.h>
#include <ll_memzone.h>
#include <ll_atomic.h>
#include <ll_pause.h>
#include <ll_debug.h>
#include <ll_log.h>

/** enqueue/dequeue behavior types */
enum ll_ring_queue_behavior {
	/** Enq/Deq a fixed number of items from a ring */
	LL_RING_QUEUE_FIXED = 0,
	/** Enq?Deq as many items as possible form ring */
	LL_RING_QUEUE_VARIABLE,
};

/** prod/cons sync同步 类型*/
enum ll_ring_sync_type {
	LL_RING_SYNC_MT,	//多线程安全 （多消费者，多生产者）
	LL_RING_SYNC_ST,	//单线程安全
	LL_RING_SYNC_MT_RTS, //多线程relaxed tail 同步
	LL_RING_SYNC_MT_HTS, //多线程 head/tail 同步
};

#define LL_RING_MZ_PREFIX "LR_"
#define LL_RING_NAMESIZE (LL_MEMZONE_NAMESIZE -	\
			sizeof(LL_RING_MZ_PREFIX) + 1)

/*
 * 对于不同同步类型 headtail结构有所不同
 * 但是sync_type 和tail的保持间距一致
 * */
struct	ll_ring_headtail {
	volatile uint32_t head; //prod/consumer head
	volatile uint32_t tail; //prod/consumer tail
	LL_STD_C11
	union {
		//prod/cons sync type
		enum ll_ring_sync_type sync_type;
		//表示这是一个单线程安全的
		uint32_t single;
	};
};
/**
 * relaxed tail sync(rts)
 * */
union __ll_ring_rts_poscnt {
	//使得写pos和cnt是一个原子操作
	uint64_t raw __ll_aligned(8);
	struct {
		uint32_t cnt; //参考计数
		uint32_t pos; //head/tail 所在位置
	} val;
};

union __ll_ring_hts_pos {
	//使得写head和tail是一个原子操作
	uint64_t raw __ll_aligned(8);
	struct {
		uint32_t head; //参考计数
		uint32_t tail; //head/tail 所在位置
	} pos;
};

struct ll_ring_rts_headtail {
	volatile union __ll_ring_rts_poscnt tail;
	enum ll_ring_sync_type sync_type; //prod/cons sync type
	uint32_t htd_max; //max of the distance between head/tail
	volatile union __ll_ring_rts_poscnt head;
};

struct ll_ring_hts_headtail {
	volatile union __ll_ring_hts_pos ht;
	enum ll_ring_sync_type sync_type; //prod/cons sync type
};

struct ll_ring {
	//name of the ring
	char name[LL_RING_NAMESIZE] __ll_cache_aligned;
	//flags supplied at creation
	int flags;
	//容纳ll_ring的memzone
	const struct ll_memzone *memzone;
	
	uint32_t size;
	//mask of ring (size - 1)
	uint32_t mask;
	//可用长度
	uint32_t capacity;

	char pad0 __ll_cache_aligned; //empty cache line

	//ring producer status;
	LL_STD_C11
	union {
		struct ll_ring_headtail prod;
		struct ll_ring_hts_headtail hts_prod;
		struct ll_ring_rts_headtail rts_prod;
	}__ll_cache_aligned;

	char pad1 __ll_cache_aligned; //empty cache line

	//ring consumer status;
	LL_STD_C11
	union {
		struct ll_ring_headtail cons;
		struct ll_ring_hts_headtail hts_cons;
		struct ll_ring_rts_headtail rts_cons;
	}__ll_cache_aligned;
	
	char pad2 __ll_cache_aligned; //empty cache line
};

#define RING_F_SP_ENQ 0x0001 //默认单供应者入队
#define RING_F_SC_DEQ 0x0002 //默认单消费者出队

#define RING_F_EXACT_SZ 0x0004 

#define LL_RING_SZ_MASK (0x7fffffffU) //mask of ring size

#define RING_F_MP_RTS_ENQ 0x0008 //the default enqueue is MP RTS
#define RING_F_MC_RTS_DEQ 0x0010 //the default dequeue is MC RTS


#define RING_F_MP_HTS_ENQ 0x0020 //the default enqueue is MP HTS
#define RING_F_MC_HTS_DEQ 0x0040 //the default dequeue is MC HTS 


#ifdef __cplusplus
}
#endif

#endif
