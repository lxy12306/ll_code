#ifndef _LL_MEMZONE_H_
#define _LL_MEMZONE_H_


#define LL_MEMZONE_2MB            0x00000001   /**< Use 2MB pages. */
#define LL_MEMZONE_1GB            0x00000002   /**< Use 1GB pages. */
#define LL_MEMZONE_16MB           0x00000100   /**< Use 16MB pages. */
#define LL_MEMZONE_16GB           0x00000200   /**< Use 16GB pages. */
#define LL_MEMZONE_256KB          0x00010000   /**< Use 256KB pages. */
#define LL_MEMZONE_256MB          0x00020000   /**< Use 256MB pages. */
#define LL_MEMZONE_512MB          0x00040000   /**< Use 512MB pages. */
#define LL_MEMZONE_4GB            0x00080000   /**< Use 4GB pages. */
#define LL_MEMZONE_SIZE_HINT_ONLY 0x00000004   /**< Use available page size */
#define LL_MEMZONE_IOVA_CONTIG    0x00100000   /**< Ask for IOVA-contiguous memzone. */



struct ll_memzone {
#define LL_MEMZONE_NAMESIZE 32 /*memzone名字的最大长度*/
	char name[LL_MEMZONE_NAMESIZE]; /*memzone的名字*/
	
	ll_iova_t iova;	//起始IO地址
	LL_STD_C11
	union {
		void *addr; //起始虚拟地址
		uint64_t addr_64; //强制64字节
	};
	size_t len;  //该memzone的长度
	
	uint64_t hugepage_sz; //该memzone所在页面长度

	int32_t socket_id; //NUMA处理器ID

	uint32_t flags; //该memzone的characteristics
}__ll_aligned;

#define LL_IS_MEMZONE_IN_ONE_PAGE(memzone) \
	((struct ll_memzone*(memzone))->hugepage_sz == ((uint64_t)-1))

//申请对齐内存
const struct ll_memzone *ll_memzone_reserve_aligned(const char *name,
			size_t len,int socket_id,
			unsigned flags, unsigned align);

static inline const struct ll_memzone *
ll_memzone_reserve(const char *name,
			size_t len,int socket_id,
			unsigned flags) {
	return  ll_memzone_reserve_aligned(name, len, socket_id, flags, 0);
}


//释放申请的内存
int
ll_memzone_free(const struct ll_memzone *mz);


#endif
