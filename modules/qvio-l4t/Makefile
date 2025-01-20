KERNELRELEASE ?= $(shell uname -r)
KERNEL_DIR ?= /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)
KARGS ?=

obj-m += qvio.o
qvio-objs := \
	module.o \
	device.o \
	cdev.o \
	umods.o

qvio-objs += \
	video.o \
	queue.o

qvio-objs += pci_device.o
qvio-objs += platform_device.o

ccflags-y += \
-O3 \
-DQVIO_MODULE_VERSION=\"$(MODULE_VERSION)\"

all:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) modules

dtbs:
	dtc -O dtb -@ qvio-l4t.dts -o qvio-l4t.dtbo

clean:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) clean
