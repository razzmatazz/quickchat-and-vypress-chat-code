#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>

int passes(char * buf, unsigned buf_len)
{
	assert(buf && buf_len);

	return buf[0]!='1' && buf[0]!='0' && buf[0]!='M' && buf[0]!='C';
}

void normalize(unsigned char * buf, unsigned buf_len)
{
	unsigned n;
	for(n=0; n < buf_len; n++) {
		if(buf[n] < 32) {
			buf[n] = '.';
			continue;
		}
		if(buf[n] > 127) {
			buf[n] = '.';
			continue;
		}
	}
}

void print_normalized(char * buf, unsigned buf_len)
{
	normalize((unsigned char*)buf, buf_len);

	buf[buf_len] = '\0';
	puts(buf);
}

void print_source(struct sockaddr_in * sa)
{
	assert(sa);

	printf("%08lx", (unsigned long)ntohl(sa->sin_addr.s_addr));
}

int main(int argc, char ** argv)
{
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in sa;
	char * buf;
	unsigned sz, sa_len;

	if(argc!=2) {
		fputs("must specify port\n", stderr);
		exit(EXIT_FAILURE);
	}

	buf = malloc(1024+1); /* +1 for '\0' */
	if(!buf) { perror("malloc"); exit(1); }

	if((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))<0) {
		perror("socket"); exit(1);
	}

	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(atoi(argv[1]));
	if(bind(sock, (struct sockaddr*)&sa, sizeof(sa))==-1) {
		perror("bind"); exit(1);
	}
	
	while(1) {
		sz = recvfrom(sock, (void*)buf, 1024, 0,
			(struct sockaddr*)&sa, &sa_len);

		if(sz>0) {
			if(passes(buf, sz))
				print_normalized(buf, sz);
	//		print_source(&sa);
		}
	}

	return 0;
}

