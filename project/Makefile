CC=gcc
CFLAGS= -Wall -Wextra -Werror -pedantic -std=gnu99 -g -fsanitize=address -Wno-unused-variable -Wno-unused-parameter

all: clean build

default: build

build: server.c client.c 
	${CC} -o server server.c ${CFLAGS}
	${CC} -o client client.c ${CFLAGS}

clean:
	rm -rf server client *.bin *.out *.dSYM

zip: clean
	rm -f project0.zip
	mkdir -p project
	cp server.c client.c Makefile project
	zip project0.zip project/*
