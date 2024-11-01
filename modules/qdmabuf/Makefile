KERNELRELEASE ?= $(shell uname -r)
KERNEL_DIR ?= /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)
KARGS ?=

obj-m += qdmabuf.o
qdmabuf-objs := \
	module.o \
	device.o \
	cdev.o \
	ioctl.o \
	dmabuf_exp.o \
	dmabuf_exp_dma_contig.o \
	dmabuf_exp_dma_sg.o \
	dmabuf_exp_vmalloc.o

ccflags-y += \
-DQDMABUF_MODULE_VERSION=\"$(MODULE_VERSION)\"

all:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) modules

dtbs:
	dtc -O dtb -@ qdmabuf.dts -o qdmabuf.dtbo

clean:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) clean
