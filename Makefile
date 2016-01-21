

all: udp-sender udp-receiver

clean:
	rm -f udp-sender udp-receiver

udp-sender: udp-sender.c
	gcc -Wall -O3 -o udp-sender udp-sender.c -lrt

udp-receiver: udp-receiver.c
	gcc -Wall -O3 -o udp-receiver udp-receiver.c -lrt
