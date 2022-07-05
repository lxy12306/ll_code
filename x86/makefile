.PHONY= clean

CFLAG=-g -Wall 
CC=gcc

test_atomic:test_atomic.c
	$(CC) $(CFLAG) $^ -o $@ -lpthread -I ../include

clean:
	rm -f test_atomic
