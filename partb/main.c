#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char *argv[]) {
	struct addrinfo hints, *res, *p;
	int status;
	char ipstr[INET_ADDRSTRLEN];
	if (argc != 2) {
		fprintf(stderr,"usage: showip hostname\n");
		return 1;
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 2;
	}
	printf("IP addresses for %s:\n\n", argv[1]);
	for(p = res;p != NULL; p = p->ai_next) {
		void *addr;
		// get the pointer to the address itself,
		// different fields in IPv4 and IPv6:
 		struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
		addr = &(ipv4->sin_addr);

		// convert the IP to a string and print it:
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
		printf(" %s: %s\n", "IPv4", ipstr);
	}
	freeaddrinfo(res); // free the linked list
	return 0;
}