test: walloc.c main.c walloc.h
	gcc -Wall -Wextra walloc.c main.c -o test

clean:
	rm -f test
