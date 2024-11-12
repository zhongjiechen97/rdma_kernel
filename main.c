#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "common.h"

int __init rdma_kern_init(void);
void __exit rdma_kern_exit(void);

static ssize_t rdma_kern_write(struct file * file, 
		const char __user * buffer,
		size_t count, loff_t *ppos) {
	ssize_t size = count;
	char *cmd = kzalloc(count, GFP_KERNEL);
	int err = 0;
	int argc;
	char **argv;

	if(!cmd) {
		err_info("allocate cmd error\n");
		size = -ENOMEM;
		goto err_kzalloc;
	}

	if(copy_from_user(cmd, buffer, count)) {
		err_info("copy_from_user error\n");
		size = -EFAULT;
		goto err_copy;
	}

	argc = str2arg(cmd, &argv);
	if(argc < 0) {
		err_info("str2arg error\n");
		size = argc;
		goto err_str2arg;
	}

	err = run_proc(argc, argv);
	if(err < 0) {
		err_info("run_proc error. err: %d\n", err);
		size = err;
	}

	kfree(argv);
err_str2arg:
err_copy:
	kfree(cmd);
err_kzalloc:
	return size;
}

struct proc_ops rdma_kern_fops = {
	.proc_write		= rdma_kern_write,
};

struct proc_dir_entry *rdma_kern_ent;

int __init rdma_kern_init(void) {
	int ret = 0;
	rdma_kern_ent = proc_create("rdma_kern", 0666, 
			NULL, &rdma_kern_fops);
	if(!rdma_kern_ent) {
		err_info("Cannont create proc entry\n");
		ret = -ENOMEM;
	}
	return ret;
}

void __exit rdma_kern_exit(void) {
	remove_proc_entry("rdma_kern", NULL);
}

module_init(rdma_kern_init);
module_exit(rdma_kern_exit);
MODULE_LICENSE("GPL");
