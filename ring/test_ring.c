#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <mutex>
#include <set>

#include <ll_ring.h>

#define DUMP_FILE "test_ring_dump_file.txt"


#define TEST_CHOISE_SP_SC 0
#define TEST_CHOISE_MP_MC 1
#define TEST_CHOISE_RTS 2

#define TEST_CHOISE TEST_CHOISE_RTS

#define SOCKET_ID_ANY -1

#define MY_STRUCT_NUM (1<<6)
struct my_struct {
	int index;
	int belong_to;
} g_my_struct[MY_STRUCT_NUM];

void init_my_struct(int index_begin, int index_end, int belong_to) {
	int i;
	if (index_end > MY_STRUCT_NUM) {
		index_end = MY_STRUCT_NUM;
	}
	for (i = index_begin; i < index_end; ++i) {
		g_my_struct[i].index = i;
		g_my_struct[i].belong_to = belong_to;
	}
}
void print_my_struct() {
	for (int i = 0; i < MY_STRUCT_NUM; ++i) {
		if (i !=0 && i % 4 == 0) {
			printf("\n");
		}
		printf("[%d]{index = %d, belong_to = %x} ", i, g_my_struct[i].index, g_my_struct[i].belong_to);
	}
}

struct ll_ring *r = NULL;

void* prod1(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int i, ret;
	struct my_struct *p;
	for (i = 0; i < MY_STRUCT_NUM;) {
		p = &g_my_struct[i];
		p->index = i;
		p->belong_to = (1<<8 | 1);
		ret = ll_ring_enqueue(r, p);
		if (ret == 0) {
			++i;
		}
	}
	return NULL;
}

void* cons1(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int i, ret;
	struct my_struct *p;
	for(i = 0; i < MY_STRUCT_NUM;) {
		ret = ll_ring_dequeue(r, (void **)&p);
		if (ret == 0) {
			if ((p)->index != i) {
				fprintf(stderr, "en and de error dequeue index = %d\n", (p)->index);
				return (void *)p;
			}
			printf("index = %d,", (p)->index);
			(p)->belong_to = (2<<8 | 1);
			++i;
		}
	}
	printf("\n");
	return NULL;
}

void test_sp_sc(FILE * f_dump) {
	struct my_struct *a_my_struct[MY_STRUCT_NUM];
	int ret;
	pthread_t tid_cons = {0};
	pthread_t tid_prod = {0};

	r = ll_ring_create("my_ring", MY_STRUCT_NUM, SOCKET_ID_ANY, RING_F_SP_ENQ|RING_F_SC_DEQ|RING_F_EXACT_SZ);
	if (r == NULL) {
		return;
	}

	ll_ring_dump(f_dump, r);
	
	ll_ring_enqueue(r, g_my_struct);
	ll_ring_dump(f_dump, r);
	
	ll_ring_dequeue(r, (void **)a_my_struct);
	if (a_my_struct[0] != g_my_struct) {
		fprintf(stderr, "en and de error\n");
	}
	ll_ring_dump(f_dump,r);
	
	ret = pthread_create(&tid_prod, NULL, prod1, (void*)r);
	if (ret < 0) {
		goto test_exit;
	}
	ret = pthread_create(&tid_cons, NULL, cons1, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod, NULL);
		goto test_exit;
	}

	pthread_join(tid_cons, NULL);
	pthread_join(tid_prod, NULL);
	print_my_struct();
test_exit:
	ll_ring_free(r);
}

std::mutex mutex;
int g_index = 0;

void* prod3(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int idx_begin, idx_end, ret;
	for (;;) {
		mutex.lock();
		idx_begin = g_index;
		idx_end = g_index + 1;
		if (idx_end <= MY_STRUCT_NUM) {
			g_index = idx_end;
		}
		mutex.unlock();
		if (idx_end <= MY_STRUCT_NUM) {
			struct my_struct *a[1];
			a[0] = &g_my_struct[idx_begin];
			(a[0])->belong_to = (1 << 8 | 2);
__insert:
			ret = ll_ring_enqueue_bulk(r, (void **)a, 1, NULL);
			if (ret == 0) {
				goto __insert;
			}
		} else {
			break;
		}
	}
	return NULL;
}

void* prod2(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int idx_begin, idx_end, ret;
	for (;;) {
		mutex.lock();
		idx_begin = g_index;
		idx_end = g_index + 2;
		if (idx_end <= MY_STRUCT_NUM) {
			g_index = idx_end;
		}
		mutex.unlock();
		usleep(5);
		if (idx_end <= MY_STRUCT_NUM) {
			struct my_struct *a[2];
			a[0] = &g_my_struct[idx_begin];
			(a[0])->belong_to = (1 << 8 | 3);
			a[1] = &g_my_struct[idx_begin + 1];
			(a[1])->belong_to = (1 << 8 | 3);
__insert:
			ret = ll_ring_enqueue_bulk(r, (void **)a, 2, NULL);
			if (ret == 0) {
				goto __insert;
			}
		} else {
			break;
		}
	}
	return NULL;
}

std::set<int> set;
std::mutex mutex2;

void* cons3(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int i = 0;
	int ret;
	for (;;) {
			struct my_struct *a[1];
__insert:
			usleep(10);
			ret = ll_ring_dequeue_bulk(r, (void **)a, 1, NULL);
			if (ret == 0) {
				if (++i < 10)
					goto __insert;
				else 
					break;
			}
			a[0]->belong_to |= (2 << 8 | 2) << 16;
			mutex2.lock();	
			set.emplace(a[0]->index);
			mutex2.unlock();
			usleep(10);
	}
	return NULL;
}

void* cons2(void *arg) {
	struct ll_ring *r = (struct ll_ring *)arg;
	int i = 0;
	int ret;
	for (;;) {
			struct my_struct *a[2];
__insert:
			usleep(10);
			ret = ll_ring_dequeue_bulk(r, (void **)a, 2, NULL);
			if (ret == 0) {
				if (++i < 10)
					goto __insert;
				else 
					break;
			}
			a[0]->belong_to |= (2 << 8 | 3) << 16;
			a[1]->belong_to |= (2 << 8 | 3) << 16;
			mutex2.lock();	
			set.emplace(a[0]->index);
			set.emplace(a[1]->index);
			mutex2.unlock();
	}
	return NULL;
}

void test_mp_mc(FILE * f_dump) {
	int ret;
	pthread_t tid_prod2 = {0}, tid_prod3 = {0};
	pthread_t tid_cons2 = {0}, tid_cons3 = {0};


	r = ll_ring_create("my_ring", MY_STRUCT_NUM, SOCKET_ID_ANY, RING_F_EXACT_SZ);
	if (r == NULL) {
		return;
	}

	ll_ring_dump(f_dump, r);
	
	ret = pthread_create(&tid_prod3, NULL, prod3, (void*)r);
	if (ret < 0) {
		goto test_exit;
	}
	ret = pthread_create(&tid_prod2, NULL, prod2, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod3, NULL);
		goto test_exit;
	}
	ret = pthread_create(&tid_cons2, NULL, cons2, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod2, NULL);
		pthread_join(tid_prod3, NULL);
		goto test_exit;
	}
	ret = pthread_create(&tid_cons3, NULL, cons3, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod2, NULL);
		pthread_join(tid_prod3, NULL);
		pthread_join(tid_cons2, NULL);
		goto test_exit;
	}
	pthread_join(tid_cons2, NULL);
	pthread_join(tid_cons3, NULL);
	pthread_join(tid_prod2, NULL);
	pthread_join(tid_prod3, NULL);
	print_my_struct();
	for (auto value : set) {
		printf("%d ",value);
	}
	printf("\n %ld \n",set.size());
	ll_ring_dump(f_dump, r);
test_exit:
	ll_ring_free(r);
}

#define test_mp_mc_rts_prod3 1
#define test_mp_mc_rts_cons3 1
void test_mp_mc_rts(FILE * f_dump) {
	int ret;
	pthread_t tid_prod2 = {0}, tid_cons2 = {0};
#if test_mp_mc_rts_prod3
	pthread_t tid_prod3 = {0};
#endif

#if test_mp_mc_rts_cons3
	pthread_t tid_cons3 = {0};
#endif

	r = ll_ring_create("my_ring", MY_STRUCT_NUM, SOCKET_ID_ANY, RING_F_EXACT_SZ| RING_F_MC_RTS_DEQ | RING_F_MP_RTS_ENQ);
	if (r == NULL) {
		return;
	}

	ll_ring_dump(f_dump, r);
	
	ret = pthread_create(&tid_prod2, NULL, prod2, (void*)r);
	if (ret < 0) {
		goto test_exit;
	}
#if test_mp_mc_rts_prod3
	ret = pthread_create(&tid_prod3, NULL, prod3, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod2, NULL);
		goto test_exit;
	}
#endif
	ret = pthread_create(&tid_cons2, NULL, cons2, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod2, NULL);
#if test_mp_mc_rts_prod3
		pthread_join(tid_prod3, NULL);
#endif
		goto test_exit;
	}
#if test_mp_mc_rts_cons3
	ret = pthread_create(&tid_cons3, NULL, cons3, (void*)r);
	if (ret < 0) {
		pthread_join(tid_prod2, NULL);
#if test_mp_mc_rts_prod3
		pthread_join(tid_prod3, NULL);
#endif
		pthread_join(tid_cons2, NULL);
		goto test_exit;
	}
#endif
	pthread_join(tid_cons2, NULL);
#if test_mp_mc_rts_cons3
	//pthread_join(tid_cons3, NULL);
#endif
	pthread_join(tid_prod2, NULL);
#if test_mp_mc_rts_prod3
	//pthread_join(tid_prod3, NULL);
#endif
	print_my_struct();
	for (auto value : set) {
		printf("%d ",value);
	}
	printf("\n %ld \n",set.size());
	ll_ring_dump(f_dump, r);
test_exit:
	ll_ring_free(r);
}

int main() {
	
	FILE *f_dump = fopen(DUMP_FILE, "w+");
	if(f_dump == NULL) {
		perror("fopen error\n");
		return -1;
	}
	
	init_my_struct(0,MY_STRUCT_NUM,0);

#if TEST_CHOISE == TEST_CHOISE_SP_SC
	test_sp_sc(f_dump);
#elif TEST_CHOISE == TEST_CHOISE_MP_MC
	test_mp_mc(f_dump);
#elif TEST_CHOISE == TEST_CHOISE_RTS
	test_mp_mc_rts(f_dump);
#endif
	if (fclose(f_dump)) {
		perror("fclose error\n");
	}

	return 0;
}
