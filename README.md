# Kernel RDMA application

## Download MLNX OFED driver
```shell
wget https://content.mellanox.com/ofed/MLNX_OFED-24.10-0.7.0.0/MLNX_OFED_LINUX-24.10-0.7.0.0-ubuntu22.04-x86_64.tgz
tar -xvf MLNX_OFED_LINUX-24.10-0.7.0.0-ubuntu22.04-x86_64.tgz
cd MLNX_OFED_LINUX-24.10-0.7.0.0-ubuntu22.04-x86_64/src
tar -xvf MLNX_OFED_SRC-24.10-0.7.0.0.tgz
cd MLNX_OFED_SRC-24.10-0.7.0.0/SOURCES
tar -xvf mlnx-ofed-kernel_24.10.OFED.24.10.0.7.0.1.orig.tar.gz
```

## Build the repository
```shell
make -j`nproc` configure
make -j`nproc`
```

