#include <stdio.h>
#include <stdlib.h>
#include "ll_mem_op_64.h"

void * test_1() {
	return NULL;
}

int main() {
	char *p = (char *)malloc(5);
	*(p + 4) = '\0';
	char *q = ll_memset_short_64(p, 'a', 4);
	printf("%s", p);
	printf("%p", p);
	printf("%s", q);
	return 0;
}
