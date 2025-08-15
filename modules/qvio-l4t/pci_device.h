#ifndef __QVIO_PCI_DEVICE_H__
#define __QVIO_PCI_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/pci.h>

#include "cdev.h"
#include "dma_block.h"
#include "zdev.h"
#include "qdma_wr.h"

struct qvio_pci_device {
	struct kref ref;

	struct device *dev;
	struct platform_device *pdev;
	uint32_t device_id;
	uint32_t device_subid;

	// PCI specific
	struct pci_dev *pci_dev;
	void __iomem *bar[6];
	int regions_in_use;
	int got_regions;
	int msi_enabled;
	int msix_enabled;
	int irq_lines[8];
	int irq_counter;

	// DMA block for test
	struct dma_pool* desc_pool;
	struct dma_block_t dma_blocks[8];

	// vars for dev-attr
	int dma_block_index;
	int dma_sync;
	int test_case;

	struct qvio_zdev* zdev;
	void __iomem * reg_intr;
	struct qvio_qdma_wr* qdma_wr;
};

int qvio_pci_device_register(void);
void qvio_pci_device_unregister(void);

struct qvio_pci_device* qvio_pci_device_new(void);
struct qvio_pci_device* qvio_pci_device_get(struct qvio_pci_device* self);
void qvio_pci_device_put(struct qvio_pci_device* self);

#endif // __QVIO_PCI_DEVICE_H__