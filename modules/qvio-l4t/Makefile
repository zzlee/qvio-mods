KERNELRELEASE ?= $(shell uname -r)
KERNEL_DIR ?= /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)
KARGS ?=

obj-m += qvio.o
qvio-objs := \
	module.o \
	device.o \
	cdev.o \
	umods.o \
	buf_entry.o \
	dma_block.o \
	utils.o \
	video_queue.o \
	zdev.o \
	qdma_wr.o \
	qdma_rd.o \
	tpg.o

qvio-objs += \
	video.o \
	queue.o

qvio-objs += pci_device.o
# qvio-objs += platform_device.o

ccflags-y += \
-O3 \
-DQVIO_MODULE_VERSION=\"$(MODULE_VERSION)\"

ccflags-y += -I$(src)/../include
ccflags-y += -I$(src)/../vitis/video_common_v4_10/src
ccflags-y += -I${src}/../vitis/v_tpg_v8_3/src
qvio-objs += ../vitis/v_tpg_v8_3/src/xv_tpg.o
qvio-objs += ../vitis/v_tpg_v8_3/src/xv_tpg_g.o
qvio-objs += ../vitis/v_tpg_v8_3/src/xv_tpg_sinit.o

all:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) modules

dtbs:
	dtc -O dtb -@ qvio-l4t.dts -o qvio-l4t.dtbo

clean:
	$(MAKE) -C $(KERNEL_DIR) $(KARGS) M=$(PWD) clean
