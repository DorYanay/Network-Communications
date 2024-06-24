# Makefile for TCP project

all: Receiver Sender

tcp-server: Receiver.c
	gcc -o Receiver Receiver.c

tcp-client: Sender.c
	gcc -o Sender Sender.c

clean:
	rm -f *.o Receiver Sender

runs:
	./Receiver

runc:
	./Sender

runs-strace:
	strace -f ./Receiver

runc-strace:
	strace -f ./Sender

