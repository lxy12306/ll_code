.PHONY:clean all
OBJ=memzone.o mcfg.o errno.o config.o debug.o lcore.o fbarray.o
TEST_LCORE_APPLY_OBJ=../log/ll_log.o mcfg.o config.o lcore.o private.o errno.o
TEST_OBJ=test/test_lcore
all:$(OBJ)
test:$(TEST_OBJ)

private.o:common_private.c
	gcc -g -Wall -c $^ -o $@ -I ../include
fbarray.o:common_fbarray.c
	gcc -g -Wall -c $^ -o $@ -I ../include
memzone.o:common_memzone.c
	gcc -g -Wall -c $^ -o $@ -I ../include
config.o:common_config.c
	gcc -g -Wall -c $^ -o $@ -I ../include
mcfg.o:common_mcfg.c
	gcc -g -Wall -c $^ -o $@ -I ../include
errno.o:common_error.c
	gcc -g -Wall -c $^ -o $@ -I ../include
debug.o:common_debug.c
	gcc -g -Wall -c $^ -o $@ -I ../include
lcore.o:common_lcore.c
	gcc -g -Wall -c $^ -o $@ -I ../include
test/test_lcore:test/test_lcore.c $(TEST_LCORE_APPLY_OBJ)
	gcc -g -Wall $^ -o $@ -I ../include
clean:
	rm -rf *.o
