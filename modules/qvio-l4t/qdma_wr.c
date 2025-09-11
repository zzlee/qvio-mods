#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include "qdma_wr.h"
#include "uapi/qvio-l4t.h"
#include "xdma_desc.h"
#include "utils.h"

static struct qvio_cdev_class __cdev_class;
static const unsigned int __reset_delay = 100;

static void __free(struct kref *ref);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait);
static int __buf_entry_from_sgt(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry);
static int __start_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry);
static int __streamon(struct qvio_video_queue* self);
static int __streamoff(struct qvio_video_queue* self);
static int __reset_cores(struct qvio_qdma_wr* self);

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
	.poll = __file_poll,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
};

int qvio_qdma_wr_register(void) {
	int err;

	pr_info("\n");

	err = qvio_cdev_register(&__cdev_class, 0, 255, "qdma_wr");
	if(err) {
		pr_err("qvio_cdev_register() failed\n");
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_qdma_wr_unregister(void) {
	pr_info("\n");

	qvio_cdev_unregister(&__cdev_class);
}

struct qvio_qdma_wr* qvio_qdma_wr_new(void) {
	int err;
	struct qvio_qdma_wr* self = kzalloc(sizeof(struct qvio_qdma_wr), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	self->video_queue = qvio_video_queue_new();
	if(! self->video_queue) {
		pr_err("qvio_video_queue_new() failed\n");
		err = -ENOMEM;
		goto err1;
	}

	self->video_queue->parent = self;
	self->video_queue->buf_entry_from_sgt = __buf_entry_from_sgt;
	self->video_queue->start_buf_entry = __start_buf_entry;
	self->video_queue->streamon = __streamon;
	self->video_queue->streamoff = __streamoff;

	return self;

err1:
	kfree(self);
err0:
	return NULL;
}

struct qvio_qdma_wr* qvio_qdma_wr_get(struct qvio_qdma_wr* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __free(struct kref *ref) {
	struct qvio_qdma_wr* self = container_of(ref, struct qvio_qdma_wr, ref);

	// pr_info("\n");

	qvio_video_queue_put(self->video_queue);
	kfree(self);
}

void qvio_qdma_wr_put(struct qvio_qdma_wr* self) {
	if (self)
		kref_put(&self->ref, __free);
}

int qvio_qdma_wr_probe(struct qvio_qdma_wr* self) {
	int err;

	self->video_queue->dev = self->dev;
	self->video_queue->device_id = self->device_id;

	self->desc_pool = dma_pool_create("qdma_wr", self->dev, PAGE_SIZE, 32, 0);
	if(!self->desc_pool) {
		pr_err("dma_pool_create() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	err = __reset_cores(self);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
		goto err1;
	}

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev, &__cdev_class);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err2;
	}

	return 0;

err2:
err1:
	dma_pool_destroy(self->desc_pool);
err0:
	return err;
}

void qvio_qdma_wr_remove(struct qvio_qdma_wr* self) {
	qvio_cdev_stop(&self->cdev, &__cdev_class);
	dma_pool_destroy(self->desc_pool);
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_qdma_wr* self = filp->private_data;

	ret = qvio_video_queue_file_ioctl(self->video_queue, filp, cmd, arg);
	if(ret == -ENOSYS) {
		switch(cmd) {
		// TODO:

		default:
			pr_err("unexpected, cmd=%d\n", cmd);
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait) {
	struct qvio_qdma_wr* self = filp->private_data;

	return qvio_video_queue_file_poll(self->video_queue, filp, wait);
}

irqreturn_t qvio_qdma_wr_irq_handler(int irq, void *dev_id) {
	int err;
	struct qvio_qdma_wr* self = dev_id;
	uintptr_t reg = (uintptr_t)self->reg;
	u32 value;
	struct qvio_buf_entry* buf_entry;

#if 1
	value = io_read_reg(reg, 0x0C); // ISR (ap_done)
	if(! (value & 0x01)) {
		// pr_warn("unexpected, value=%u\n", value);
		return IRQ_NONE;
	}

	io_write_reg(reg, 0x0C, value & 0x01); // ap_done, TOW

#if 0
	pr_info("QDMA-WR, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	err = qvio_video_queue_done(self->video_queue, &buf_entry);
	if(err) {
		pr_err("qvio_video_queue_done() failed, err=%u\n", err);
		goto err0;
	}

	if(! buf_entry) {
		pr_warn("unexpected value, buf_entry=%llX\n", (int64_t)buf_entry);
		goto err0;
	}

#if 0
	{
		struct dma_block_t* pDmaBlock;
		struct xdma_desc* pSgdmaDesc;

		pDmaBlock = &buf_entry->desc_blocks[0];
		pSgdmaDesc = pDmaBlock->cpu_addr;
		pr_info("reg[0x00]=0x%x dsc_adr 0x%llx, dsc_adj %u, dst_addr %x:%x\n",
			io_read_reg(reg, 0x00), buf_entry->dsc_adr, buf_entry->dsc_adj, (u32)pSgdmaDesc->dst_addr_hi, (u32)pSgdmaDesc->dst_addr_lo);
	}
#endif

#if 1
	// try to do another job
	io_write_reg(reg, 0x10, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
	io_write_reg(reg, 0x14, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
	io_write_reg(reg, 0x18, buf_entry->dsc_adj);
	io_write_reg(reg, 0x00, 0x01); // ap_start
#endif
#endif

err0:
	return IRQ_HANDLED;
}

static int __buf_entry_from_sgt(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry) {
	int err;
	struct qvio_qdma_wr* qdma_wr = self->parent;
	struct scatterlist* sg;
	int nents;
	size_t buffer_size;
	struct dma_block_t* pDmaBlock;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	dma_addr_t nxt_addr;
	ssize_t dma_len;
	ssize_t sg_bytes;
	int i, adj_descs;

	buf_entry->dev = qdma_wr->dev;
	buf_entry->desc_pool = qdma_wr->desc_pool;

	sg = sgt->sgl;
	nents = sgt->nents;

	if(nents >= PAGE_SIZE / sizeof(struct xdma_desc)) {
		err = -EINVAL;
		pr_err("unexpected value, nents=%d\n", nents);
		goto err0;
	}

	err = utils_calc_buf_size(&self->format, buf->offset, buf->stride, &buffer_size);
	if(err < 0) {
		pr_err("utils_calc_buf_size() failed, err=%d\n", err);
		goto err0;
	}
#if 0
	pr_info("buffer_size=%lu\n", buffer_size);
#endif

	pDmaBlock = &buf_entry->desc_blocks[0];
	err = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
	if(err) {
		pr_err("qvio_dma_block_alloc() failed, ret=%d\n", err);
		goto err0;
	}

	pSgdmaDesc = pDmaBlock->cpu_addr;
	src_addr = 0xA0000000;
	sg_bytes = 0;
	for (i = 0; i < nents; i++, sg = sg_next(sg)) {
		dst_addr = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);
		nxt_addr = pDmaBlock->dma_handle + ((u8*)(pSgdmaDesc + 1) - (u8*)pDmaBlock->cpu_addr);

		pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
		pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
		pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
		pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
		pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
		pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));

		sg_bytes += dma_len;
		src_addr += dma_len;

#if 0
		pr_info("%d: dma_len=%lu, sg_bytes=%lu\n", i, dma_len, sg_bytes);
#endif

		if(sg_bytes >= buffer_size) {
			pSgdmaDesc->bytes = cpu_to_le32(dma_len - (sg_bytes - buffer_size));
			pSgdmaDesc->next_lo = 0;
			pSgdmaDesc->next_hi = 0;
			adj_descs = i;
			break;
		}

		pSgdmaDesc->bytes = cpu_to_le32(dma_len);

		pSgdmaDesc++;
	}

	pSgdmaDesc = pDmaBlock->cpu_addr;
	for(i = 0;i < adj_descs;i++, pSgdmaDesc++) {
#if 0
		pr_warn("%d: bytes %u, src_addr 0x%llx, dst_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d\n",
			i, le32_to_cpu(pSgdmaDesc->bytes),
			((u64)(le32_to_cpu(pSgdmaDesc->src_addr_hi)) << 32) | le32_to_cpu(pSgdmaDesc->src_addr_lo),
			((u64)(le32_to_cpu(pSgdmaDesc->dst_addr_hi)) << 32) | le32_to_cpu(pSgdmaDesc->dst_addr_lo),
			((u64)(le32_to_cpu(pSgdmaDesc->next_hi)) << 32) | le32_to_cpu(pSgdmaDesc->next_lo),
			adj_descs - i);
#endif

		pSgdmaDesc->control = cpu_to_le32(XDMA_DESC_MAGIC | ((adj_descs - i) << 8));
	}

#if 0
	pr_warn("%d: bytes %u, src_addr 0x%llx, dst_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d\n",
		i, le32_to_cpu(pSgdmaDesc->bytes),
		((u64)(le32_to_cpu(pSgdmaDesc->src_addr_hi)) << 32) | le32_to_cpu(pSgdmaDesc->src_addr_lo),
		((u64)(le32_to_cpu(pSgdmaDesc->dst_addr_hi)) << 32) | le32_to_cpu(pSgdmaDesc->dst_addr_lo),
		((u64)(le32_to_cpu(pSgdmaDesc->next_hi)) << 32) | le32_to_cpu(pSgdmaDesc->next_lo),
		adj_descs - i);
#endif

	pSgdmaDesc->control = cpu_to_le32(XDMA_DESC_MAGIC | XDMA_DESC_STOPPED);

	buf_entry->dsc_adr = pDmaBlock->dma_handle;
	buf_entry->dsc_adj = adj_descs;
#if 0
	pr_warn("---- dsc_adr 0x%llx, dsc_adj %u\n", buf_entry->dsc_adr, buf_entry->dsc_adj);
#endif

	dma_sync_single_for_device(buf_entry->dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_TO_DEVICE);

	return 0;

err0:
	return err;
}

static int __start_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry) {
	int err;
	struct qvio_qdma_wr* qdma_wr = self->parent;
	uintptr_t reg = (uintptr_t)qdma_wr->reg;
	struct dma_block_t* pDmaBlock;
	struct xdma_desc* pSgdmaDesc;
	size_t buffer_size;

	err = utils_calc_buf_size0(&self->format, &buffer_size);
	if(err < 0) {
		pr_err("utils_calc_buf_size0() failed, err=%d\n", err);
		goto err0;
	}

	pr_info("%08X %d x %d, %lu\n", self->format.fmt, self->format.width, self->format.height, buffer_size);

	pDmaBlock = &buf_entry->desc_blocks[0];
	pSgdmaDesc = pDmaBlock->cpu_addr;
	pr_info("reg[0x00]=0x%x dsc_adr 0x%llx, dsc_adj %u, dst_addr %x:%x\n",
		io_read_reg(reg, 0x00), buf_entry->dsc_adr, buf_entry->dsc_adj, (u32)pSgdmaDesc->dst_addr_hi, (u32)pSgdmaDesc->dst_addr_lo);

	io_write_reg(reg, 0x10, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
	io_write_reg(reg, 0x14, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
	io_write_reg(reg, 0x18, buf_entry->dsc_adj);
	io_write_reg(reg, 0x1C, buffer_size);
	io_write_reg(reg, 0x00, 0x01); // ap_start

	pr_info("QDMA WR started...\n");

	return 0;

err0:
	return err;
}

static int __streamon(struct qvio_video_queue* self) {
	int err;
	struct qvio_qdma_wr* qdma_wr = self->parent;
	uintptr_t reg = (uintptr_t)qdma_wr->reg;

	err = __reset_cores(qdma_wr);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
		goto err0;
	}

	pr_info("reg[0x00]=0x%x\n", io_read_reg(reg, 0x00));
	io_write_reg(reg, 0x00, 0x00);
	io_write_reg(reg, 0x04, 0x01); // GIE
	io_write_reg(reg, 0x08, 0x01); // IER (ap_done)

	return 0;

err0:
	return err;
}

static int __streamoff(struct qvio_video_queue* self) {
	int err;
	struct qvio_qdma_wr* qdma_wr = self->parent;
	uintptr_t reg = (uintptr_t)qdma_wr->reg;
	u32 value;
	int i;

	io_write_reg(reg, 0x04, 0x00); // GIE
	io_write_reg(reg, 0x08, 0x00); // IER (ap_done)

	for(i = 0;i < 5;i++) {
		value = io_read_reg(reg, 0x00);
		if(value & 0x04) // ap_idle
			break;

		pr_info("reg[0x00]=0x%x\n", value);
		msleep(100);
	}

	err = __reset_cores(qdma_wr);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

static int __reset_cores(struct qvio_qdma_wr* self) {
	int err;
	struct qvio_zdev* zdev = self->zdev;

	pr_info("reset qdma_wr...\n");
	err = qvio_zdev_reset_mask(zdev, self->reset_mask, __reset_delay);
	if(err < 0) {
		pr_err("qvio_zdev_reset_mask() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}
