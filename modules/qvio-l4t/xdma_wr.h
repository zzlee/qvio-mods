#ifndef __QVIO_XDMA_WR_H__
#define __QVIO_XDMA_WR_H__

#include <linux/platform_device.h>
#include <linux/irqreturn.h>

#include "cdev.h"
#include "video_queue.h"
#include "dma_block.h"

struct qvio_xdma_wr {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;
	struct qvio_cdev cdev;

	void __iomem * reg;
	int channel;
	struct mutex* mutex_irq_block;
	struct qvio_video_queue* video_queue;
	int irq_counter;
	struct dma_pool* desc_pool;
};

// register
int qvio_xdma_wr_register(void);
void qvio_xdma_wr_unregister(void);

// object alloc
struct qvio_xdma_wr* qvio_xdma_wr_new(void);
struct qvio_xdma_wr* qvio_xdma_wr_get(struct qvio_xdma_wr* self);
void qvio_xdma_wr_put(struct qvio_xdma_wr* self);

// device probe
int qvio_xdma_wr_probe(struct qvio_xdma_wr* self);
void qvio_xdma_wr_remove(struct qvio_xdma_wr* self);

irqreturn_t qvio_xdma_wr_irq_handler(int irq, void *dev_id);

// test cases
int qvio_xdma_wr_test_case_0(struct qvio_xdma_wr* self, struct dma_block_t* dma_blocks);

#endif // __QVIO_XDMA_WR_H__
