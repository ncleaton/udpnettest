/*

Read the timings and length of udp packets from stdin, and send random data packets
with the correct size and timing to the specified host and port.

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
#include <errno.h>
#include "stdint.h"

//################################################################################

#define MSGBUFSIZE 100000

#define TIMESPEC_GT(ts1, ts2) ( ((ts1).tv_sec > (ts2).tv_sec) || ((ts1).tv_sec == (ts2).tv_sec && (ts1).tv_nsec > (ts2).tv_nsec) )


struct sendspec {
	struct timespec when;
	size_t len;
};

struct sendspec*
read_all_sendspecs(FILE *f, unsigned long long start_at_time_ns)
{
	struct sendspec *vec;
	int vec_len, vec_capacity, got;
	unsigned long long timestamp, timestamp_offset =0;
	struct timespec ts;

	vec_capacity = 1024;
	vec = malloc( vec_capacity * sizeof(*vec) );
	if (vec == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	vec_len = 0;

	for (;;) {
		if (vec_len >= vec_capacity) {
			vec_capacity *= 2;
			vec = realloc( vec, vec_capacity * sizeof(*vec) );
			if (vec == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(1);
			}
		}

		got = scanf("%llu %lu", &timestamp, &( vec[vec_len].len ));
		if (got == EOF) {
			// install terminator at end of vector
			vec[vec_len].len = 0;
			vec[vec_len].when.tv_sec = 0;
			vec[vec_len].when.tv_nsec = 0;
			break;
		}
		if (got != 2) {
			fprintf(stderr, "scanf got %d fields\n", got);
			exit(1);
		}

		if (timestamp_offset == 0 && start_at_time_ns != 0) {
			// First line of log, need to calculate the offset to apply to log times
			// to get the replay to begin at the correct time.
			if (timestamp >= start_at_time_ns) {
				fprintf(stderr, "the packet log starts after the requested start time\n");
				exit(1);
			}
			timestamp_offset = start_at_time_ns - timestamp;
		}

		timestamp += timestamp_offset;
		vec[vec_len].when.tv_nsec = timestamp % (1000*1000*1000);
		vec[vec_len].when.tv_sec = (timestamp - vec[vec_len].when.tv_nsec) / (1000*1000*1000);

		++vec_len;
	}

	if (start_at_time_ns == 0) {
		// start as soon as possible, we just need to ensure that the first timestamp is a little in the future
		clock_gettime(CLOCK_REALTIME, &ts);
		int secs_in_future = vec[0].when.tv_sec - ts.tv_sec;
		if (secs_in_future < 2) {
			int i, secs_bump = 2 - secs_in_future;
			for ( i=0 ; i<vec_len ; i++ )
				vec[i].when.tv_sec += secs_bump;
		}
	}

	return vec;
}

int
main(int argc, char* argv[])
{
	struct sockaddr_in addr;
	struct hostent *target_he;
	char *target_name;
	int sockfd, result, port, i, packets_sent_late=0;
	unsigned long long start_at_time_ns;
	struct sendspec *ss;
	struct timespec now_ts;

	// a buffer holding a sequence number in the first 8 bytes, the rest random
	unsigned long long msgbuf_as_ull[MSGBUFSIZE / sizeof(unsigned long long) + 1];
	unsigned char *msgbuf_as_uchar = (unsigned char *) msgbuf_as_ull;
	for ( i=sizeof(msgbuf_as_ull[0]) ; i < sizeof(msgbuf_as_ull) ; i++ )
		msgbuf_as_uchar[i] = rand() % 256;
	msgbuf_as_ull[0] = 0;

	if (argc > 4 || argc < 3) {
		fprintf(stderr, "usage: %s TARGET_HOST TARGET_PORT [DELAY_UNTIL_UNIX_TIMESTAMP]\n", argv[0]);
		exit(1);
	}
	target_name = argv[1];
	port = atoi(argv[2]);
	if (argc == 4)
		start_at_time_ns = atoll(argv[3]) * 1000*1000*1000;
	else
		start_at_time_ns = 0;


	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(1);
	}
	target_he = gethostbyname(target_name);
	if (target_he == NULL) {
		fprintf(stderr, "cannot resolve %s\n", target_name);
		exit(1);
	}
	bzero((char *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	bcopy((char *)target_he->h_addr, (char *)&addr.sin_addr.s_addr, target_he->h_length);
	addr.sin_port = htons(port);

	ss = read_all_sendspecs(stdin, start_at_time_ns);
	clock_gettime(CLOCK_REALTIME, &now_ts);
	if (ss->len && TIMESPEC_GT(now_ts, ss[0].when)){
		fprintf(stderr, "it is already too late to send the first packet\n");
		exit(1);
	}
	while (ss->len) {
		if (TIMESPEC_GT(now_ts, ss->when)) {
			// this packet should have been sent even before the previous packet was actually sent, no need for a new clock_gettime call before sending it
			++packets_sent_late;
		} else {
			clock_gettime(CLOCK_REALTIME, &now_ts);
			if (TIMESPEC_GT(now_ts, ss->when)) {
				++packets_sent_late;
			} else {
				// not yet time to send this one
				for (;;) {
					result = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(ss->when), NULL);
					if (result == 0)
						break;
					else if (result != EINTR) {
						perror("clock_nanosleep");
						exit(1);
					}
				}
			}
		}

		result = sendto(sockfd, msgbuf_as_uchar, ss->len, 0, (struct sockaddr *) &addr, sizeof(addr));
		if (result < 0) {
			perror("sendto");
			exit(1);
		}

		// increment sent packet sequence number, ready for next send
		msgbuf_as_ull[0]++;

		ss++;
	}

	printf("%llu packets sent, %d too late\n", msgbuf_as_ull[0], packets_sent_late);
	return 0;
}
