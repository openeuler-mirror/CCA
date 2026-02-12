KERNEL_DIR ?= /usr/src/kernels/$(shell uname -r)
MODULE_DIR := $(shell pwd)

ccflags-y +=	-I$(PWD) \
		-I$(srctree)/drivers/crypto/hisilicon/

obj-m += rme_acc.o
rme_acc-objs:= rme_acc_drv.o rme_sec.o rme_hpre.o rme_zip.o rme_dae.o

all:
	$(MAKE) modules -C $(KERNEL_DIR) M=$(MODULE_DIR)

clean:
	$(MAKE) clean -C $(KERNEL_DIR) M=$(MODULE_DIR)