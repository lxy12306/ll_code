

#include <time.h>
#include <stdio.h>


#include <inttypes.h>

uint64_t rdtsc() {
	unsigned int lo, hi;
	__asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;
}


int main() {

	struct timespec time1 = {0};
	
	clock_gettime(CLOCK_MONOTONIC, &time1);
	
	printf("CLOCK_MONITONIC:(%lu, %lu)\n", time1.tv_sec, time1.tv_nsec);

	uint64_t time2 = rdtsc();

	printf("CLOCK_MONITONIC:(%lu)\n", time2);

	return 0;

}
