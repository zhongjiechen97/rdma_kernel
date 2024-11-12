MLNX_OFED_KERNEL := /users/chenzj/MLNX_OFED_LINUX-24.10-0.7.0.0-ubuntu22.04-x86_64/src/MLNX_OFED_SRC-24.10-0.7.0.0/SOURCES/mlnx-ofed-kernel-24.10.OFED.24.10.0.7.0.1

ifneq ($(KERNELRELEASE),)
	rdma_kern-objs := main.o common.o kernel_core.o
	obj-$(CONFIG_INFINIBAND) := rdma_kern.o
	EXTRA_CFLAGS+=-D__KERNEL_PROC $(extra_macro)
else
	PWD:=$(shell pwd)
	njobs:=1

all:
	$(MAKE) -C $(MLNX_OFED_KERNEL) M=$(PWD)
	$(MAKE) -C ./user_app/

.PHONY: configure
configure:
	$(MLNX_OFED_KERNEL)/configure --with-core-mod -j$(njobs)

.PHONY: clean
clean:
	$(MAKE) -C $(MLNX_OFED_KERNEL) M=$(PWD) clean
ifneq ($(shell ls Module.symvers 2> /dev/null),)
	rm $(foreach i, $(shell ls Module.symvers),$(i))
endif
	$(MAKE) -C ./user_app clean
endif
