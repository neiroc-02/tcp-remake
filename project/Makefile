CC=g++
CFLAGS= 

all: clean build

default: build

build: server.o client.o packet_utils.o
	${CC} -o server server.o packet_utils.o ${CFLAGS}
	${CC} -o client client.o packet_utils.o ${CFLAGS}

server.o: server.cpp packet_utils.h
	${CC} -c server.cpp ${CFLAGS}

client.o: client.cpp packet_utils.h
	${CC} -c client.cpp ${CFLAGS}

packet_utils.o: packet_utils.cpp packet_utils.h
	${CC} -c packet_utils.cpp ${CFLAGS}

clean:
	rm -rf server client *.o *.bin *.out *.dSYM

zip: clean
	rm -f project0.zip
	mkdir -p project
	cp server.cpp client.cpp packet_utils.h Makefile project
	zip project0.zip project/*
