#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include "common.h"

int str2arg(char *cmd, char ***argv) {
	int i;
	int argc = 0;
	int len = strlen(cmd);
	int cur_p = 0;
	bool is_space = 0;

	for(i = 0; i < len; i++) {
		if(cmd[i] != ' ' && cmd[i] != '\n' && !is_space) {
			is_space = 1;
			argc++;
		}
		else if(cmd[i] == ' ' || cmd[i] == '\n') {
			is_space = 0;
			cmd[i] = 0;
		}
	}

	*argv = kzalloc(sizeof(char**)*argc, GFP_KERNEL);
	if(!(*argv)) {
		argc = -ENOMEM;
		goto err_alloc;
	}

	is_space = 0;
	for(i = 0; i < len; i++) {
		if(cmd[i] != 0 && !is_space) {
			is_space = 1;
			(*argv)[cur_p++] = cmd + i;
		}
		else if(cmd[i] == 0) {
			is_space = 0;
		}
	}

err_alloc:
	return argc;
}

enum param_index {
	PARAM_ISSERVER		= 0,
	PARAM_DEVNAME,
	PARAM_SERVADDR,
	PARAM_PORTNUM,
	PARAM_RDMAPORT,
	PARAM_SGIDINDEX,
	PARAM_MAXINDEX,
};

static int run_kernel_rdma(int argc, char **argv) {
	bool is_server = 0;
	struct sockaddr_in s_addr;
	unsigned short portnum;
	int rdma_port;
	int sgid_index;

	if(argc < PARAM_MAXINDEX) {
		return -EINVAL;
	}

	sscanf(argv[PARAM_PORTNUM], "%d", &portnum);
	sscanf(argv[PARAM_RDMAPORT], "%d", &rdma_port);
	sscanf(argv[PARAM_SGIDINDEX], "%d", &sgid_index);

	if(!strcmp(argv[PARAM_ISSERVER], "server")) {
		is_server = 1;
	}

	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(portnum);
	if(is_server)
		s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		s_addr.sin_addr.s_addr = in_aton(argv[PARAM_SERVADDR]);

	return kernel_rdma_core(is_server, argv[PARAM_DEVNAME], &s_addr,
				rdma_port, sgid_index);
}

#define help_out(fmt, args...)						\
	printk(KERN_NOTICE fmt, ##args)

#define help_info()							\
	help_out("%s help info: \n", THIS_MODULE->name);		\
	help_out("help: Print this info\n");				\
	help_out("server/client: Run %s as server/client. "		\
		"Command format: "					\
		"server/client [dev_name] [serveraddr] [portnum] "	\
		"[sgid_index]\n",					\
		THIS_MODULE->name);

int run_proc(int argc, char **argv) {
	int err = 0;
	if(!strcmp(argv[0], "help")) {
		help_info();
	}
	else if(!strcmp(argv[0], "server") ||
			!strcmp(argv[0], "client")) {
		err = run_kernel_rdma(argc, argv);
	}
	else {
		err_info("Invalid argument.\n");
		help_info();
		err = -EINVAL;
	}

	return err;
}
