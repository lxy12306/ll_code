#include <ll_log.h>
#include <ll_errno.h>
#include <ll_mempool.h>

struct student {
    char name[0x20];
    int age;
    char xueli[0x100];
    char work[0x100];  
};

int test1() {
    struct ll_mempool *mp;
    unsigned student_num = 100;
    unsigned cache_size = 10;
    int ret;
	mp = ll_mempool_create("student", student_num, sizeof(struct student), cache_size, 0, NULL, NULL, NULL, NULL, -1, LL_MEMPOOL_F_SC_GET|LL_MEMPOOL_F_SP_PUT|LL_MEMPOOL_F_NO_IOVA_CONTIG|LL_MEMPOOL_F_NO_SPREAD);
    
    if (mp == NULL) {
        LL_LOG(ERR, USER1, "can not create student mempool, errno = %d", ll_errno);
        return  -1;
    }

	struct student * s1 = NULL;
	ret = ll_mempool_get(mp, (void**)&s1);
	if (ret != 0) {
		LL_LOG(INFO, USER1, "get obj failed!");
		ll_mempool_free(mp);
	}
	
	ll_memcpy(s1->name, "lxy", sizeof("lxy"));

	struct student * s2 = NULL;
	ret = ll_mempool_get(mp, (void**)&s2);
	if (ret != 0) {
		LL_LOG(INFO, USER1, "get obj failed!");
		ll_mempool_free(mp);
	}
	ll_memcpy(s2->name, "hwq", sizeof("hwq"));
	
	ll_mempool_put(mp, (void *)s2);
	ll_mempool_put(mp, (void *)s1);

    ll_mempool_free(mp);
    return 0;
}


int test2() {
    struct ll_mempool *mp;
    unsigned student_num = 2;
    unsigned cache_size = 0;
    int ret;
	mp = ll_mempool_create("student", student_num, sizeof(struct student), cache_size, 0, NULL, NULL, NULL, NULL, -1, LL_MEMPOOL_F_SC_GET|LL_MEMPOOL_F_SP_PUT|LL_MEMPOOL_F_NO_SPREAD);
    
    if (mp == NULL) {
        LL_LOG(ERR, USER1, "can not create student mempool, errno = %d", ll_errno);
        return  -1;
    }

	struct student * s1 = NULL;
	ret = ll_mempool_get(mp, (void**)&s1);
	if (ret != 0) {
		LL_LOG(INFO, USER1, "get obj failed!");
		ll_mempool_free(mp);
	}
	
	ll_memcpy(s1->name, "lxy", sizeof("lxy"));

	struct student * s2 = NULL;
	ret = ll_mempool_get(mp, (void**)&s2);
	if (ret != 0) {
		LL_LOG(INFO, USER1, "get obj failed!");
		ll_mempool_free(mp);
	}
	ll_memcpy(s2->name, "hwq", sizeof("hwq"));

	struct student * s3 = NULL;
	ret = ll_mempool_get(mp, (void**)&s3);
	if (ret == 0) {
		LL_LOG(INFO, USER1, "mempool error!");
		ll_mempool_free(mp);
	}

	ll_mempool_put(mp, (void *)s2);
	ll_mempool_put(mp, (void *)s1);

    ll_mempool_free(mp);
    return 0;
}

int main() {
	//test1();
	test2();
	return 0;
}
