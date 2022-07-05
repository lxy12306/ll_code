.PHONY:clean all test

COMMON_RELYON_OBJ=../common/config.o ../common/errno.o ../common/debug.o ../log/ll_log.o
RING_RELYON_OBJ=../common/memzone.o
RING_OBJ=ll_ring.o
all:$(RING_OBJ)

test:test_ring
test_ring:test_ring.c ll_ring.o $(RING_OBJ) $(RING_RELYON_OBJ) $(COMMON_RELYON_OBJ)
	g++ -g -Wall -O0 $^ -o $@ -I ../include -lpthread
ll_ring.o:ll_ring.c
	gcc -g -Wall -c $^ -o $@ -I ../include
clean:
	rm *.o test_ring
