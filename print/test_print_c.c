#include "ll_print_c.h"

void test_2_weidu(const char *mem);

int main() {
	int i = 10;
	char ai_test[100];
	for(int i = 0; i < 100; ++i) {
		ai_test[i] = i;
	}
	//ll_print_vec_dw(ai_test,i);
	//ll_print_vec_char(ai_test,50);
	//ll_print_vec_b(ai_test,50);
	//ll_print_vec_qw(ai_test,5);
	printf("%*s%#-20llx\n",10,"",(long long)i);
	//	
	test_2_weidu(ai_test);
}
//void ll_fprint_vec_two(FILE *output_file, const char *mem,const char* mode, int mode_width, int i, int j);
void test_2_weidu(const char *mem) {
	ll_fprint_vec_two(stdout, mem, "d", 10, 2, 3);
	ll_print_vecvec_b(mem, 10, 10);
	ll_print_vecvec_bound_int(mem, 5, 9);
	ll_print_vecvec_bound_ll(mem, 5, 12);
}
