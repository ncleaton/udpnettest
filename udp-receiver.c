/*

Recieve packets from udp-sender, and write nanosecond timestamps and packet
sequence numbers to stdout.

*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include "stdint.h"

//################################################################################

unsigned long long
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000*1000*1000 * ts.tv_sec + ts.tv_nsec;
}

#define MSGBUFSIZE 100000

int
main(int argc, char* argv[])
{
	char buf[MSGBUFSIZE];
	struct sockaddr_in addr, cli_addr;
	int sockfd, listen_port, optval;
	unsigned long long sequence, timestamp;

	if (argc != 2) {
		fprintf(stderr, "usage: %s LISTEN_PORT\n", argv[0]);
		exit(1);
	}
	listen_port = atoi(argv[1]);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(1);
	}

	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval));

	bzero((char *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((unsigned short)listen_port);
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}

	while (1) {
		socklen_t addrlen = sizeof(cli_addr);
		int recv_bytes = recvfrom(sockfd, buf, MSGBUFSIZE, 0, (struct sockaddr*)&cli_addr, &addrlen);
		if (recv_bytes < 0) {
			perror("recvfrom");
			exit(1);
		}
		timestamp = now_ns();

		if (recv_bytes >= sizeof(sequence)) {
			bcopy(buf, (char *)&sequence, sizeof(sequence));
			printf("%llu %llu\n", timestamp, sequence);
			fflush(stdout);
		}
	}

	return 0;
}
