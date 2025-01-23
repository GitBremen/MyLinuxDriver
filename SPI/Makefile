ARCH := arm64
obj-m += mcp2515.o
KDIR := /home/caster/rk3566/lubancat1n_sdk/kernel
PWD ?= $(shell pwd)
# CROSS_COMPILE := aarch64-linux-gnu- lubancat2_defconfig
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order .*.*.cmd .*.*.mod .*.*.d .*.mod
