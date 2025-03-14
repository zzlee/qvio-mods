#ifndef __QVIO_DEVICE_H__
#define __QVIO_DEVICE_H__

#include "cdev.h"
#include "video.h"

#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/cdev.h>

struct dma_block_t {
	size_t size;
	void *cpu_addr;
	dma_addr_t dma_handle;
	gfp_t gfp;
};

struct qvio_device {
	struct kref ref;

	struct device *dev;
	struct platform_device *pdev;

	// PCIs device
	struct pci_dev *pci_dev;
	uint32_t device_id;
	void __iomem *bar[6];
	int regions_in_use;
	int got_regions;
	int msi_enabled;
	int msix_enabled;
	int irq_lines[32];
	int irq_counter;

	// DMA block for test
	struct dma_block_t dma_blocks[8];

	// Char device
	struct qvio_cdev cdev;
};

struct qvio_device* qvio_device_new(void);
struct qvio_device* qvio_device_get(struct qvio_device* self);
void qvio_device_put(struct qvio_device* self);

#endif // __QVIO_DEVICE_H__
