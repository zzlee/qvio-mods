#ifndef __QVIO_QDMA_WR_H__
#define __QVIO_QDMA_WR_H__

#include <linux/platform_device.h>
#include <linux/irqreturn.h>

#include "cdev.h"
#include "video_queue.h"

struct qvio_qdma_wr {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;
	struct qvio_cdev cdev;

	void __iomem * reg_zdev;
	void __iomem * reg;
	struct qvio_video_queue* video_queue;
	int irq_counter;
	struct dma_pool* desc_pool;
};

// register
int qvio_qdma_wr_register(void);
void qvio_qdma_wr_unregister(void);

// object alloc
struct qvio_qdma_wr* qvio_qdma_wr_new(void);
struct qvio_qdma_wr* qvio_qdma_wr_get(struct qvio_qdma_wr* self);
void qvio_qdma_wr_put(struct qvio_qdma_wr* self);

// device probe
int qvio_qdma_wr_probe(struct qvio_qdma_wr* self);
void qvio_qdma_wr_remove(struct qvio_qdma_wr* self);

irqreturn_t qvio_qdma_wr_irq_handler(int irq, void *dev_id);

#endif // __QVIO_QDMA_WR_H__
