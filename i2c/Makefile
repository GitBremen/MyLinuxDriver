ARCH := arm64
obj-m += driver_frame.o
KDIR := /home/caster/rk3566/lubancat_offical/lubancat1n_sdk/kernel
PWD ?= $(shell pwd)
# CROSS_COMPILE := aarch64-linux-gnu- lubancat2_defconfig
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order .*.*.cmd .*.*.mod .*.*.d .*.mod
