#ifndef __QVIO_XDMA_RD_H__
#define __QVIO_XDMA_RD_H__

#include <linux/platform_device.h>
#include <linux/irqreturn.h>

#include "cdev.h"
#include "video_queue.h"

struct qvio_xdma_rd {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;
	struct qvio_cdev cdev;

	struct qvio_video_queue* video_queue;
	int irq_counter;
	struct dma_pool* desc_pool;
};

// register
int qvio_xdma_rd_register(void);
void qvio_xdma_rd_unregister(void);

// object alloc
struct qvio_xdma_rd* qvio_xdma_rd_new(void);
struct qvio_xdma_rd* qvio_xdma_rd_get(struct qvio_xdma_rd* self);
void qvio_xdma_rd_put(struct qvio_xdma_rd* self);

// device probe
int qvio_xdma_rd_probe(struct qvio_xdma_rd* self);
void qvio_xdma_rd_remove(struct qvio_xdma_rd* self);

irqreturn_t qvio_xdma_rd_irq_handler(int irq, void *dev_id);

#endif // __QVIO_XDMA_RD_H__
