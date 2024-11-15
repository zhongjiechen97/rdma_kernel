BEAR := bear --

rdma_kern-objs := main.o common.o kernel_core.o
obj-$(CONFIG_INFINIBAND) := rdma_kern.o
EXTRA_CFLAGS+=-D__KERNEL_PROC $(extra_macro)

all:
	$(BEAR) make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules -j`nproc`

.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
