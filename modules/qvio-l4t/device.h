#ifndef __QVIO_DEVICE_H__
#define __QVIO_DEVICE_H__

#include "cdev.h"
#include "video.h"
#include "buf_entry.h"

#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/cdev.h>

#include "xaximm_test0.h"
#include "xaximm_test1.h"
#include "xz_frmbuf_writer.h"
#include "xv_tpg.h"

enum qvio_state {
	QVIO_STATE_READY,
	QVIO_STATE_START
};

struct qvio_device {
	struct kref ref;

	struct device *dev;
	struct platform_device *pdev;

	// PCIs device
	struct pci_dev *pci_dev;
	uint32_t device_id;
	uint32_t device_subid;
	void __iomem *bar[6];
	int regions_in_use;
	int got_regions;
	int msi_enabled;
	int msix_enabled;
	int irq_lines[8];
	int irq_counter;

	// DMA block for test
	int desc_block_size;
	struct dma_pool* desc_pool;
	struct dma_block_t dma_blocks[8];

	// vars for dev-attr
	int dma_block_index;
	int dma_sync;
	int test_case;

	// Char device
	struct qvio_cdev cdev;

	// IP cores
	void __iomem * zzlab_env;
	XAximm_test0 xaximm_test0;
	XAximm_test1 xaximm_test1;
	XZ_frmbuf_writer xFrmBufWr;
	XV_tpg xTpg;
	XAximm_test0 xaximm_test2; // same control interface as test0
	XAximm_test0 xaximm_test3; // same control interface as test0
	XAximm_test0 xaximm_test2_1; // same control interface as test0
	void __iomem * qdma_intr;
	void __iomem * qdma_wr;
	void __iomem * qdma_rd;

	int work_mode;
	struct qvio_format format;
	int planes;
	__u32 buffers_count;
	struct qvio_buffer* buffers;
	void* mmap_buffer;

	spinlock_t lock;
	struct list_head job_list; // qvio_buf_entry
	struct list_head done_list; // qvio_buf_entry
	enum qvio_state state;

	// irq control
	wait_queue_head_t irq_wait;
};

struct qvio_device* qvio_device_new(void);
struct qvio_device* qvio_device_get(struct qvio_device* self);
void qvio_device_put(struct qvio_device* self);

#endif // __QVIO_DEVICE_H__
