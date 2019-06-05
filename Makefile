CC=gcc
OS=UNIX
BIN=main

loader: *.c
	$(CC) -o $(BIN) *.c -D$(OS) -Wall -g

memcheck:
	valgrind --leak-check=yes ./$(BIN) -s
