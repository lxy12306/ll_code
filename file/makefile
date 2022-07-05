.PHONY:clean all
OBJ=ll_file.o
all:$(OBJ)

ll_file.o:unix/eal_unix_file.c
	gcc -g -Wall -c $^ -o $@ -I ../include
clean:
	rm -rf *.o