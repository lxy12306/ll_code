.PHONY:clean all test

COMMON_RELYON_OBJ=../common/config.o ../common/errno.o ../common/debug.o ../log/ll_log.o
MEMPOOL_RELYON_OBJ=../common/memzone.o ../common/mcfg.o ../mem/eal_mem.o ../ring/ll_ring.o
MEMPOOL_OBJ=libll_mempool.so libll_mempool.a
all:$(MEMPOOL_OBJ)

test:test_mempool
test_mempool:test_mempool.c $(COMMON_RELYON_OBJ)  $(MEMPOOL_RELYON_OBJ)
	gcc -g -Wall $^ -o $@ -I ../include -L . -lll_mempool -Wl,-rpath='$$ORIGIN'

libll_mempool.so:ll_mempool.c ll_mempool_ops.c ll_mempool_ops_default.c ll_mempool_ring.c
	gcc -g -fPIC -Wall -shared $^ -o $@ -I ../include

libll_mempool.a:ll_mempool.o ll_mempool_ops.o ll_mempool_ops_default.o ll_mempool_ring.o
	ar rcs $@ $^

ll_mempool.o:ll_mempool.c
	gcc -g -Wall -c $^ -o $@ -I ../include

ll_mempool_ops.o:ll_mempool_ops.c
	gcc -g -Wall -c $^ -o $@ -I ../include

ll_mempool_ops_default.o:ll_mempool_ops_default.c
	gcc -g -Wall -c $^ -o $@ -I ../include

ll_mempool_ring.o:ll_mempool_ring.c
	gcc -g -Wall -c $^ -o $@ -I ../include
	

clean:
	rm -rf *.o *.so test_mempool
