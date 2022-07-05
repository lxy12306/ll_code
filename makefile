.PNONY=clean all test_fbarray
COMMON=common/errno.o log/ll_log.o
FBARRAY=common/fbarray.o mem/eal_mem.o file/eal_file.o
all:test_fbarray

test_fbarray:test_fbarray.o $(COMMON) $(FBARRAY) 
		g++ -g -Wall $^ -o $@

test_fbarray.o:test_fbarray.c
		g++ -g -Wall -c $^ -o $@ -I ./include

clean:
	$(MAKE) -C ./common clean
	$(MAKE) -C ./ring clean
	rm *.o test
