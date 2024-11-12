#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common.h"

void usage(char *argv0) {
	fprintf(stderr, "Usage:\n"
		"%s -d [dev_name] -p [tcp_port] -i [ibv_port] -x [sgid_index] [servername]\n"
		"\n"
		"If a servername is specified at last, "
		"this program will run as a client connecting to the specified server. "
		"Otherwise, it will start as a server\n\n", argv0);
}

int main(int argc, char *argv[]) {
	int err = 0;
	int cur_opt;
	int is_server = 1;
	char *dev_name;
	unsigned short tcp_port;
	struct sockaddr_in s_addr;
	int rdma_port, sgid_index;

	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	while((cur_opt = getopt(argc, argv, "d:p:i:x:h")) != -1) {
		switch(cur_opt) {
		case 'd':
			dev_name = optarg;
			break;
		case 'p':
			tcp_port = atoi(optarg);
			s_addr.sin_port = htons(tcp_port);
			break;
		case 'i':
			rdma_port = atoi(optarg);
			break;
		case 'x':
			sgid_index = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			return 0;
			break;
		default:
			err_info("Invalid option\n");
			err = -EINVAL;
			usage(argv[0]);
			return err;
		}
	}

	if(optind == argc - 1) {
		is_server = 0;
		err = inet_pton(AF_INET, argv[argc-1], &s_addr.sin_addr);
		if(err <= 0) {
			err = -EINVAL;
			err_info("inet_pton error\n");
			return err;
		}
		else
			err = 0;
	}
	else if(optind < argc) {
		err_info("Invalid option\n");
		usage(argv[0]);
		err = -EINVAL;
		return err;
	}

	return user_rdma_core(is_server, dev_name, &s_addr, 
				rdma_port, sgid_index);
}
