/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * intel_pt_pkt_decoder.h: Intel Processor Trace support
 * Copyright (c) 2013-2014, Intel Corporation.
 */

#ifndef INCLUDE__INTEL_PT_PKT_DECODER_H__
#define INCLUDE__INTEL_PT_PKT_DECODER_H__

#include <stddef.h>
#include <stdint.h>

#define INTEL_PT_PKT_DESC_MAX	256

#define INTEL_PT_NEED_MORE_BYTES	-1
#define INTEL_PT_BAD_PACKET		-2

#define INTEL_PT_PSB_STR		"\002\202\002\202\002\202\002\202" \
					"\002\202\002\202\002\202\002\202"
#define INTEL_PT_PSB_LEN		16

#define INTEL_PT_PKT_MAX_SZ		16

#define INTEL_PT_VMX_NR_FLAG		1

enum intel_pt_pkt_type {
	INTEL_PT_BAD,
	INTEL_PT_PAD,
	INTEL_PT_TNT,
	/*
	Taken Not-Taken (TNT) packets:
	跟踪有条件的直接分支的“方向”(采取或未采取)。
	*/
	INTEL_PT_TIP_PGD,
	//要么是分支地址，要么是PacketEn从1->0 的标志
	INTEL_PT_TIP_PGE,
	//要么是分支地址，要么是PacketEn从0->1 的标志
	INTEL_PT_TSC,
	/*Time-Stamp Counter (TSC) packets:时间戳计数器(TSC)报文
	TSC包有助于跟踪挂钟时间，并包含部分软件可见时间戳计数器
	*/
	INTEL_PT_TMA,
	INTEL_PT_MODE_EXEC,
	INTEL_PT_MODE_TSX,
	/*
	这些包为解码器提供了重要的处理器执行信息，以便解码器能够正确地解释分解的二进制文件并跟踪日志。MODE包具有多种格式，用于指示诸如执行模式(16位、32位或64位)等细节
	*/
	INTEL_PT_MTC,
	/*
	Mini Time Counter (MTC) packets:
	MTC数据包提供壁挂钟时间的周期性指示
	*/
	INTEL_PT_TIP,
	/*
	Target IP (TIP) packets:
	TIP报文记录间接分支、异常、中断、其他分支或事件的目标IP。这些包可以包含IP，尽管可以通过消除匹配最后一个IP的大字节来压缩IP值。TIP包有多种类型
	*/
	INTEL_PT_FUP,
	/*
	Flow Update Packets (FUP):
	fup提供异步事件(中断和异常)的源IP地址，以及其他不能从二进制确定源地址的情况.
	*/
	INTEL_PT_CYC,
	/*
	Cycle Count (CYC) packets:
	传递包之间经过的cpu时钟周期数
	*/
	INTEL_PT_VMCS,
	INTEL_PT_PSB,
	/*
	Packet Stream Boundary packets：数据包流边界包
	PSB数据包充当以固定间隔(例如，每4K跟踪数据包字节)生成的“心跳”。这些包允许包解码器在输出数据流内找到包边界;PSB包应该是解码器在开始解码跟踪时寻找的第一个包。
	*/
	INTEL_PT_PSBEND,
	INTEL_PT_CBR,
	/*
	Core Bus Ratio (CBR) packets:
	CBR报文中包含核心:总线时钟率。
	*/
	INTEL_PT_TRACESTOP,
	INTEL_PT_PIP, 
	/*Paging information Packet:PIP记录对CR3寄存器的修改。
	这个信息，以及来自操作系统的关于每个进程CR3值的信息，允许调试器将线性地址归为正确的应用程序源。*/
	INTEL_PT_OVF,
	/*
	overflow packets:
	当处理器内部缓冲区溢出时，发送OVF包，导致包被丢弃。这个包通知解码器丢失，并可以帮助解码器响应这种情况
	*/
	INTEL_PT_MNT,
	INTEL_PT_PTWRITE,
	/*
	包括传递给PTWRITE指令的操作数的值
	Write Data to a Processor Trace Packet
	*/
	INTEL_PT_PTWRITE_IP,
	INTEL_PT_EXSTOP,
	INTEL_PT_EXSTOP_IP,
	INTEL_PT_MWAIT,
	INTEL_PT_PWRE,
	INTEL_PT_PWRX,
	INTEL_PT_BBP,
	/*
	Block Begin Packets :
	*/
	INTEL_PT_BIP,
	/*
	Block Item Packets:
	指示组中保存的状态值
	*/
	INTEL_PT_BEP,
	/*
	Block End Packets:
	*/
	INTEL_PT_BEP_IP,
};

struct intel_pt_pkt {
	enum intel_pt_pkt_type	type;
	int			count;
	uint64_t		payload;
};

/*
 * Decoding of BIP packets conflicts with single-byte TNT packets. Since BIP
 * packets only occur in the context of a block (i.e. between BBP and BEP), that
 * context must be recorded and passed to the packet decoder.
 */
enum intel_pt_pkt_ctx {
	INTEL_PT_NO_CTX,	/* BIP packets are invalid */
	INTEL_PT_BLK_4_CTX,	/* 4-byte BIP packets */
	INTEL_PT_BLK_8_CTX,	/* 8-byte BIP packets */
};

const char *intel_pt_pkt_name(enum intel_pt_pkt_type);

int intel_pt_get_packet(const unsigned char *buf, size_t len,
			struct intel_pt_pkt *packet,
			enum intel_pt_pkt_ctx *ctx);

void intel_pt_upd_pkt_ctx(const struct intel_pt_pkt *packet,
			  enum intel_pt_pkt_ctx *ctx);

int intel_pt_pkt_desc(const struct intel_pt_pkt *packet, char *buf, size_t len);

#endif
