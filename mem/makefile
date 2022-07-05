.PHONY:clean all test
OBJ_APPLY_ON=../common/errno.o ../common/mcfg.o ../common/config.o ../common/lcore.o ../common/fbarray.o  ../common/private.o ../log/ll_log.o ../file/ll_file.o
TEST=test/heap_init_test
OBJ=ll_unix_mem.o ll_mem.o ll_malloc.o malloc_elem.o malloc_heap.o
test:$(TEST)
all:$(OBJ)

test/heap_init_test:test/heap_init_test.c $(OBJ) $(OBJ_APPLY_ON)
	gcc -g -Wall  $^ -o $@ -I ../include
ll_unix_mem.o:unix/eal_unix_memory.c
	gcc -g -Wall -c $^ -o $@ -I ../include
ll_mem.o:eal_memory.c
	gcc -g -Wall -c $^ -o $@ -I ../include
ll_malloc.o:ll_malloc.c
	gcc -g -Wall -c $^ -o $@ -I ../include
malloc_elem.o:malloc_elem.c
	gcc -g -Wall -c $^ -o $@ -I ../include
malloc_heap.o:malloc_heap.c
	gcc -g -Wall -c $^ -o $@ -I ../include

clean:
	rm -rf *.o
