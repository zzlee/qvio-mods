#ifndef __QVIO_DEVICE_H__
#define __QVIO_DEVICE_H__

#include "cdev.h"
#include "video.h"
#include "buf_entry.h"

#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/cdev.h>

#include "xaximm_test1.h"

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
	int irq_lines[8];
	int irq_counter;

	// DMA block for test
	struct dma_pool* desc_pool;
	struct dma_block_t dma_blocks[8];

	// vars for dev-attr
	int dma_block_index;
	int test_case;

	// Char device
	struct qvio_cdev cdev;

	// IP cores
	XAximm_test1 xaximm_test1;
	void __iomem * zzlab_env;
	u32 width;
	u32 height;
	u32 fmt; // fourcc

	// job list
	spinlock_t job_list_lock;
	struct list_head job_list; // qvio_buf_entry

	// done list
	spinlock_t done_list_lock;
	struct list_head done_list; // qvio_buf_entry

	// irq control
	u32 done_jobs;
	atomic_t irq_event;
	u32 irq_event_count;
	atomic_t irq_event_data;
	wait_queue_head_t irq_wait;
};

struct qvio_device* qvio_device_new(void);
struct qvio_device* qvio_device_get(struct qvio_device* self);
void qvio_device_put(struct qvio_device* self);

#endif // __QVIO_DEVICE_H__
