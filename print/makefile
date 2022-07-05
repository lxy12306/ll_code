.PHONY:clean all test

TEST=test_print_c
PRINT_OBJ=ll_print_c.o
all:$(PRINT_OBJ)

test:$(TEST)
test_print_c:test_print_c.c $(PRINT_OBJ)
	gcc -g -Wall $^ -o $@ -I ../include

ll_print_c.o:ll_print_c.c
	gcc -g -Wall -c $^ -o $@ -I ../include
	
clean:
	rm -rf *.o $(TEST)
