obj-m += epoll.o
KDIR := /lib/modules/5.15.0-124-generic/build
PWD ?= $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order
