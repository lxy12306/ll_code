#include <ll_memory.h>

int g_i = 0;

void test_virtual_to_io_phy_addr() {
	char a[100] = {0};
	
	char *p = (char *) malloc(1000000);
	*(p + 100000) = 'a';

	ll_phys_addr_t phys_addr = ll_mem_vir2phy(a);
	printf("%p to %lx\n", a, phys_addr);
	
	phys_addr = ll_mem_vir2phy(&g_i);
	printf("%p to %lx\n", &g_i, phys_addr);

	phys_addr = ll_mem_vir2phy(p + 100000);
	printf("%p to %lx\n", p + 100000, phys_addr);

}


int main() {
	test_virtual_to_io_phy_addr();
	return 0;
}
