.PHONY:clean all test

COMMON_RELYON_OBJ=../common/config.o ../common/errno.o ../common/debug.o ../log/ll_log.o ../common/mcfg.o
PERF_VT_OBJ=util/intel_pt_decoder/intel_pt_decoder.o util/intel_pt_decoder/intel_pt_insn_decoder.o util/intel_pt_decoder/intel_pt_pkt_decoder.o util/intel_pt_decoder/intel_pt_log.o
PERF_OBJ=ll_perf.o ll_intel_vt.o $(PERF_VT_OBJ)
PERF_RELYON_OBJ=../file/ll_file.o ../mem/ll_mem.o $(PERF_PROC_RELYON_OBJ)
PERF_PROC_RELYON_OBJ=../process/ll_pmem.o
LEARN_OBJ=perf_learn1
TEST_OBJ=./test/example ./test/test_perf ./test/test_pt_analyze
XED_PATH=../git/ll_xed/kits/xed-install-base-2022-04-28-lin-x86-64/
all:$(PERF_OBJ)

test:$(LEARN_OBJ) $(TEST_OBJ) 
perf_learn1:learn1.c $(PERF_OBJ) $(PERF_RELYON_OBJ) $(COMMON_RELYON_OBJ) $(XED_PATH)lib/libxed.a
	gcc -g -Wall $^ -o $@ -I ../include
./test/example:./test/example.c
	gcc -g -Wall $^ -o $@
./test/test_perf:./test/test_perf.c $(PERF_OBJ) $(PERF_RELYON_OBJ) $(COMMON_RELYON_OBJ) $(XED_PATH)lib/libxed.a
	gcc -g -Wall $^ -o $@ -I ../include

./test/test_pt_analyze:./test/test_pt_analyze.c $(PERF_OBJ) $(PERF_RELYON_OBJ) $(COMMON_RELYON_OBJ) $(XED_PATH)lib/libxed.a
	gcc -g -Wall $^ -o $@ -I ../include

ll_perf.o:perf.c
	gcc -g -Wall -c $^ -o $@ -I ../include
ll_intel_vt.o:intel_pt.c
	gcc -g -Wall -c $^ -o $@ -I ../include
util/intel_pt_decoder/intel_pt_decoder.o:util/intel_pt_decoder/intel_pt_decoder.c
	gcc -g -Wall -c $^ -o $@ -I ../include 
util/intel_pt_decoder/intel_pt_insn_decoder.o:util/intel_pt_decoder/intel_pt_insn_decoder.c
	gcc -g -Wall -c $^ -o $@ -I ../include
util/intel_pt_decoder/intel_pt_pkt_decoder.o:util/intel_pt_decoder/intel_pt_pkt_decoder.c
	gcc -g -Wall -c $^ -o $@ -I ../include
util/intel_pt_decoder/intel_pt_log.o:util/intel_pt_decoder/intel_pt_log.c
	gcc -g -Wall -c $^ -o $@ -I ../include


clean:
	rm -rf *.o *.so $(LEARN_OBJ) $(PERF_OBJ) $(TEST_OBJ)
