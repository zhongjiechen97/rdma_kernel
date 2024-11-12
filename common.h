#ifndef __COMMON_H__
#define __COMMON_H__

/**
 * @brief EKR: Easy Kernel Rdma
 * 
 */

#ifdef __KERNEL_PROC
#include <rdma/ib_verbs.h>
#include <linux/in.h>

struct rdma_conn_param {
	u32			qpn;
	u32			psn;
	u16			lid;
	union ib_gid		gid;
};

#define dbg_info(fmt, args...)						\
	printk(KERN_NOTICE "At %s(%d): " fmt, __FILE__, __LINE__, ##args)

#define err_info(fmt, args...)						\
	printk(KERN_ERR "Err at %s(%d): " fmt, __FILE__, __LINE__, ##args)

extern int kernel_rdma_core(bool is_server, const char *dev_name,
			const struct sockaddr_in *s_addr, 
			int rdma_port, int sgid_index);
extern int str2arg(char *cmd, char ***argv);
extern int run_proc(int argc, char **argv);

#else		/* __KERNEL_PROC */

#include <infiniband/verbs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct rdma_conn_param {
	uint32_t			qpn;
	uint32_t			psn;
	uint16_t			lid;
	union ibv_gid		gid;
};

#define dbg_info(fmt, args...)						\
	printf("At %s(%d): " fmt, __FILE__, __LINE__, ##args);

#define err_info(fmt, args...)						\
	fprintf(stderr, "Err at %s(%d): " fmt, __FILE__, __LINE__, ##args);

extern int user_rdma_core(int is_server, const char *dev_name,
			const struct sockaddr_in *s_addr, 
			int rdma_port, int sgid_index);
#endif		/* __KERNEL_PROC */

#define PRINT(var) ({												\
	dbg_info("%s = %u\n", #var, var);								\
	(var);															\
})

#define CHECK(cond) ({												\
	dbg_info("CHECK %s? %s\n", #cond, (cond)? "true": "false");		\
	(cond);															\
})


/**
 * @brief This function dumps information of all ib devices
 * 
 * @param ibdev 
 * @return int 
 */
extern void ekr_dump_all_ib_devices(void);


#endif		/* __COMMON_H__ */
