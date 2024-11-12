#include <infiniband/verbs.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common.h"

#define MAXSIZE			1024

static int setup_connection(int is_server, const struct sockaddr_in *s_addr,
			int *sock, int *client_sock) {
	int *create_sock = (is_server)? sock: client_sock;
	int err = 0;
	int flag = 1;

	*create_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(*create_sock < 0) {
		err_info("Cannot create socket\n");
		err = -errno;
		goto err_socket;
	}

	err = setsockopt(*create_sock, SOL_SOCKET, SO_REUSEPORT,
				(char*)&flag, sizeof(int));
	if(err) {
		err_info("setsockopt error\n");
		goto out_release;
	}

	if(is_server) {
		err = bind(*sock, (struct sockaddr*)s_addr, sizeof(*s_addr));
		if(err) {
			err_info("bind error\n");
			goto out_release;
		}

		err = listen(*sock, 10);
		if(err) {
			err_info("listen error\n");
			goto out_release;
		}

		*client_sock = accept(*sock, NULL, NULL);
		if(*client_sock < 0) {
			err_info("accept error\n");
			err = -errno;
			goto out_release;
		}
	}
	else {
		err = connect(*client_sock, (struct sockaddr*)s_addr, sizeof(*s_addr));
		if(err) {
			err = -errno;
			err_info("connect error, err: %d\n", err);
			goto out_release;
		}
	}
	return 0;

out_release:
	close(*create_sock);
err_socket:
	return err;
}

static int exchange_info(int is_server, struct rdma_conn_param *local,
			struct rdma_conn_param *remote, int sock, int client_sock) {
	int i;
	int err = 0;
	ssize_t size;

	for(i = 0; i < 2; i++) {
		if(is_server ^ (i % 2)) {
			size = read(client_sock, remote, sizeof(*remote));
		}
		else {
			size = write(client_sock, local, sizeof(*local));
		}
		if(size < sizeof(*local)) {
			err_info("msg_error\n");
			err = -EPIPE;
			goto err_msg;
		}
	}

err_msg:
	return err;
}

static void close_connection(int is_server, int sock, int client_sock) {
	close(client_sock);
	if(is_server)
		close(sock);
}

static struct ibv_device *ctx_find_dev(const char *dev_name) {
	int num_of_dev;
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev = NULL;

	dev_list = ibv_get_device_list(&num_of_dev);
	if(num_of_dev <= 0) {
		err_info("Did not detect devices\n");
		err_info("If device exists, check if driver is up\n");
		goto err_get_list;
	}

	if(!dev_name) {
		ib_dev = dev_list[0];
	}
	else {
		for(; (ib_dev = *dev_list); ++dev_list) {
			if(!strcmp(ibv_get_device_name(ib_dev),
					dev_name)) {
				break;
			}
		}
	}

	if(!ib_dev) {
		err_info("IB device not found\n");
	}

	free(dev_list);
err_get_list:
	return ib_dev;
}

int user_rdma_core(int is_server, const char *dev_name,
			const struct sockaddr_in *s_addr, 
			int rdma_port, int sgid_index) {
	int err = 0;
	struct ibv_device *ib_dev = NULL;
	struct ibv_context *context = NULL;
	struct ibv_pd *pd = NULL;
	struct ibv_mr *mr;
	struct ibv_cq *cq;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_qp *qp;
	struct ibv_port_attr port_attr;
	union ibv_gid local_gid;
	struct rdma_conn_param local_info, remote_info;
	struct ibv_qp_attr qp_attr;
	struct ibv_sge sge_list;
	struct ibv_send_wr send_wr, *bad_send_wr;
	struct ibv_recv_wr recv_wr, *bad_recv_wr;
	struct ibv_wc wc;
	int ne;
	int attr_mask;
	char buffer[MAXSIZE];
	int sock, client_sock;

	ib_dev = ctx_find_dev(dev_name);
	if(!ib_dev) {
		err_info("Cannont find dev %s\n", dev_name);
		err = -ENODEV;
		goto err_find_dev;
	}

	context = ibv_open_device(ib_dev);
	if(!context) {
		err_info("Cannot get context for the device\n");
		err = -ENODEV;
		goto err_open_device;
	}

	pd = ibv_alloc_pd(context);
	if(!pd) {
		err_info("Cannot allocate PD\n");
		err = -ENODEV;
		goto err_alloc_pd;
	}

	mr = ibv_reg_mr(pd, buffer, MAXSIZE, IBV_ACCESS_LOCAL_WRITE);
	if(!mr) {
		err_info("Cannot allocate MR\n");
		err = -ENOMEM;
		goto err_reg_mr;
	}

	cq = ibv_create_cq(context, 1, NULL, NULL, 0);
	if(!cq) {
		err_info("Cannot create CQ\n");
		err = -ENOMEM;
		goto err_create_cq;
	}

	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	qp_init_attr.send_cq = cq;
	qp_init_attr.recv_cq = cq;
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.sq_sig_all = 1;

	qp = ibv_create_qp(pd, &qp_init_attr);
	if(!qp) {
		err_info("Cannont crate QP\n");
		err = -ENOMEM;
		goto err_create_qp;
	}

	err = ibv_query_port(context, rdma_port, &port_attr);
	if(err) {
		goto err_conn;
	}

	err = ibv_query_gid(context, rdma_port, sgid_index, &local_gid);
	if(err) {
		goto err_conn;
	}

	local_info.qpn = qp->qp_num;
	local_info.psn = 0;
	local_info.lid = port_attr.lid;
	memcpy(&local_info.gid, &local_gid, sizeof(local_gid));

	err = setup_connection(is_server, s_addr, &sock, &client_sock);
	if(err) {
		err_info("setup_connection error\n");
		goto err_conn;
	}

	err = exchange_info(is_server, &local_info, &remote_info,
					sock, client_sock);
	if(err) {
		goto err_modify_qp;
	}

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.pkey_index = 0;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.port_num = rdma_port;
	qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE;
	attr_mask = (IBV_QP_STATE | IBV_QP_PKEY_INDEX | 
				IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
	err = ibv_modify_qp(qp, &qp_attr, attr_mask);
	if(err) {
		goto err_modify_qp;
	}

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTR;
	qp_attr.path_mtu = IBV_MTU_1024;
	qp_attr.ah_attr.src_path_bits = 0;
	qp_attr.ah_attr.port_num = rdma_port;
	qp_attr.ah_attr.dlid = remote_info.lid;
	qp_attr.ah_attr.sl = 0;
	qp_attr.ah_attr.is_global = 1;
	memcpy(&qp_attr.ah_attr.grh.dgid, &remote_info.gid, sizeof(remote_info.gid));
	qp_attr.ah_attr.grh.sgid_index = sgid_index;
	qp_attr.ah_attr.grh.hop_limit = 0xff;
	qp_attr.ah_attr.grh.traffic_class = 0;
	qp_attr.dest_qp_num = remote_info.qpn;
	qp_attr.rq_psn = remote_info.psn;
	qp_attr.max_dest_rd_atomic = 0;
	qp_attr.min_rnr_timer = 12;
	attr_mask = (IBV_QP_STATE | IBV_QP_PATH_MTU |
				IBV_QP_AV | IBV_QP_DEST_QPN |
				IBV_QP_RQ_PSN | IBV_QP_MIN_RNR_TIMER |
				IBV_QP_MAX_DEST_RD_ATOMIC);
	err = ibv_modify_qp(qp, &qp_attr, attr_mask);
	if(err) {
		goto err_modify_qp;
	}

	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.sq_psn = local_info.psn;
	qp_attr.timeout = 14;
	qp_attr.retry_cnt = 7;
	qp_attr.rnr_retry = 7;
	qp_attr.max_rd_atomic = 1;
	attr_mask = (IBV_QP_STATE | IBV_QP_SQ_PSN |
			IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
			IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC);
	err = ibv_modify_qp(qp, &qp_attr, attr_mask);
	if(err) {
		goto err_modify_qp;
	}

	sge_list.addr = (uintptr_t)buffer;
	sge_list.length = MAXSIZE;
	sge_list.lkey = mr->lkey;
	if(is_server) {
		recv_wr.wr_id = 0;
		recv_wr.next = NULL;
		recv_wr.sg_list = &sge_list;
		recv_wr.num_sge = 1;
		err = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
		if(err) {
			err_info("Cannot post recv\n");
			goto err_modify_qp;
		}
	}
	else {
		strcpy(buffer, "Hello server! I'm client!");
		send_wr.wr_id = 0;
		send_wr.next = NULL;
		send_wr.sg_list = &sge_list;
		send_wr.num_sge = 1;
		send_wr.send_flags = IBV_SEND_SIGNALED;
		send_wr.opcode = IBV_WR_SEND;
		err = ibv_post_send(qp, &send_wr, &bad_send_wr);
		if(err) {
			err_info("Cannot post send\n");
			goto err_modify_qp;
		}
	}

	while(1) {
		ne = ibv_poll_cq(cq, 1, &wc);
		if(ne < 0) {
			err = -EFAULT;
			err_info("ib_poll_cq error\n");
			goto err_modify_qp;
		}

		if(ne > 0) {
			if(wc.status != IBV_WC_SUCCESS) {
				err_info("server: %d, wc not success\n", is_server);
				goto err_modify_qp;
			}
			break;
		}

		err = exchange_info(is_server, &local_info, &remote_info,
						sock, client_sock);
		if(err) {
			err_info("server: %d, Remote peer has closed connection\n", is_server);
			goto err_modify_qp;
		}
	}

	if(is_server)
		dbg_info("buffer: %s\n", buffer);

err_modify_qp:
	close_connection(is_server, sock, client_sock);
err_conn:
	ibv_destroy_qp(qp);
err_create_qp:
	ibv_destroy_cq(cq);
err_create_cq:
	ibv_dereg_mr(mr);
err_reg_mr:
	ibv_dealloc_pd(pd);
err_alloc_pd:
	ibv_close_device(context);
err_open_device:
err_find_dev:
	return err;
}
