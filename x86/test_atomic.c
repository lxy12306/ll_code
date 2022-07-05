#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <pthread.h>

#include "include/ll_atomic.h"

void look_assembly(unsigned choice) {
	uint32_t dst, exp, src;
	uint8_t ret_bool;
	dst = 1;
	exp = 2;
	src = 3;
	switch(choice) {
		case 1: {
			ret_bool = ll_atomic32_cmpset(&dst, exp, src);
			printf("ret_bool = %d", ret_bool);
			ret_bool = ll_atomic32_cmpset(&dst, exp - 1, src);
			printf("ret_bool = %d", ret_bool);
		}
		break;
		case 2: {
			ll_atomic32_exchange(&dst, src);
		}
	}
}
uint32_t g_dst_1 = 0;

void* test_cmpxchg_1(void *arg) {
	int max = 10;
	uint32_t exp;
	exp = 0;
	uint8_t ret= 0;
	while(max) {
		if (ret = ll_atomic32_cmpset(&g_dst_1, exp, exp + 1)) {
			printf("exp = %u", exp);
			--max;
			exp += 2;
		}
	}
	return NULL;
}

void* test_cmpxchg_2(void *arg) {
	int max = 10;
	uint32_t exp;
	exp = 1;
	uint8_t ret= 0;
	while(max) {
		if (ret = ll_atomic32_cmpset(&g_dst_1, exp, exp + 1)) {
			printf("exp = %u", exp);
			--max;
			exp += 2;
		}
	}
	return NULL;
}

int main() {
	int ret;
	pthread_t tid1, tid2;
	ret = pthread_create(&tid1, NULL, test_cmpxchg_1, NULL);
	if (ret != 0) {
		printf("pthread_create error error_code=%d \n", ret);
	}
	
	ret = pthread_create(&tid2, NULL, test_cmpxchg_2, NULL);
	if (ret != 0) {
		printf("pthread_create error error_code=%d \n", ret);
		pthread_join(tid1, NULL);
	}
	
	pthread_join(tid1, NULL);
	pthread_join(tid1, NULL);
	return 0;
};
