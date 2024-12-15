obj-m += ds18b20.o
KDIR := /home/caster/rk3566/lubancat1n_sdk/kernel
PWD ?= $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order .*.*.cmd .*.*.mod .*.*.d .*.mod
