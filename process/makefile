.PNONY=clean all test
COMMON_RELYON_OBJ=../common/config.o ../common/errno.o ../common/debug.o ../log/ll_log.o ../common/mcfg.o
TEST=test/hello test/test_ptrace
TEST_RELYON_OBJ=../print/ll_print_c.o
PMEM_RELYON_OBJ=../file/ll_file.o
PMEM_OBJ=ll_pmem.o
XED_PATH=../git/ll_xed/kits/xed-install-base-2022-04-28-lin-x86-64/
all:$(PMEM_OBJ)
test:$(TEST)

test/hello:test/hello.c
	gcc -g -Wall $^ -o $@

test/test_ptrace:test/test_ptrace.c $(PMEM_OBJ) $(COMMON_RELYON_OBJ) $(PMEM_RELYON_OBJ) $(TEST_RELYON_OBJ) $(XED_PATH)lib/libxed.a
	gcc -g -Wall $^ -o $@ -I ../include -I $(XED_PATH)include/xed

ll_pmem.o:ll_pmem.c
	gcc -g -Wall -c $^ -o $@ -I ../include -I $(XED_PATH)include/xed

clean:
	rm $(TEST)
	rm $(PMEM_OBJ)
