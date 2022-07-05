#include <assert.h>

#include <ll_log.h>
#include <ll_errno.h>
#include <ll_fbarray.h>


void test_1(struct ll_fbarray* arr) {
	int idx = 0;
	long long *data =(long long*)arr->data;
	idx = ll_fbarray_find_next_n_free(arr, 0, 7);
	
	assert(idx == 0);
	ll_fbarray_set_used(arr, idx);	
	data[idx] = 'a';
		
	idx = ll_fbarray_find_next_n_free(arr, 2, 7);
	assert(idx == 2);
	ll_fbarray_set_used(arr, idx + 6);	
	data[idx] = 'b';
	
	idx = ll_fbarray_find_next_n_free(arr, 2, 7);
	assert(idx == 9);
	ll_fbarray_set_used(arr, idx + 6);	
	data[idx] = 'c';
	
	ll_fbarray_set_used(arr, 65);
	idx = ll_fbarray_find_next_n_free(arr, 2, 65);
	assert(idx == 66);
	ll_fbarray_set_used(arr, idx + 64);	
	
	
	idx = ll_fbarray_find_next_n_free(arr, 2, 33);
	
	idx = ll_fbarray_find_next_n_free(arr, 2, 255);

	idx = ll_fbarray_find_next_n_free(arr, 900, 110);
	assert(idx == -1);
	
	idx = ll_fbarray_find_next_n_free(arr, 970, 40);
	assert(idx == -1);

	idx = ll_fbarray_find_prev_n_free(arr, 970, 40);
	assert(idx == 931);

	idx = ll_fbarray_find_prev_n_free(arr, 970, 5);
	assert(idx == 966);
	
	
	idx = ll_fbarray_find_prev_n_free(arr, 970, 200);
}


int main() {
    struct ll_fbarray arr = {0};
    int ret = 0;
    ret = ll_fbarray_init(&arr, "my_arr", 1000, sizeof(long long));
    if (ret != 0) {
        LL_LOG(INFO, USER1, "ll_fbarray_init error, error_code = %d", ll_errno);
    }
   
	test_1(&arr);
    ll_fbarray_destroy(&arr);
	return 0;
}
