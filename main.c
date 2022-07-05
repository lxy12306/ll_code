#include <ll_ring.h>

int main() {
	struct ll_ring * ring = ll_ring_create("123", 100, -1, 0x3);
	ll_ring_free(ring);
	return 0;
}
