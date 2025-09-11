#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include "xdma_rd.h"
#include "uapi/qvio-l4t.h"
#include "xdma_desc.h"
#include "utils.h"

#if 0
static irqreturn_t __irq_handler_xdma(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	u32* zzlab_env;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_h2c_sgdma;
	u32 usr_irq_req, usr_int_pend;
	u32 intr;
	int i, intr_mask;
	u32 engine_int_req, engine_int_pend;
	u32 Status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("XDMA, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	zzlab_env = (u32*)self->zzlab_env;
	xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));

	usr_irq_req = xdma_irq_block[0x40 >> 2];
	usr_int_pend = xdma_irq_block[0x48 >> 2];
	intr = zzlab_env[0x28 >> 2];
	engine_int_req = xdma_irq_block[0x44 >> 2];
	engine_int_pend = xdma_irq_block[0x4C >> 2];

	// pr_info("usr_irq_req=0x%X usr_int_pend=0x%X, intr=0x%X\n", usr_irq_req, usr_int_pend, intr);
	for(i = 0;i < 4;i++) {
		intr_mask = 1 << i;

		if(usr_irq_req & intr_mask) {
			pr_info("User Interrupt %d\n", i);

			intr &= ~intr_mask;

			zzlab_env[0x28 >> 2] = intr;
		}
	}

	// pr_info("engine_int_req=0x%X engine_int_pend=0x%X\n", engine_int_req, engine_int_pend);
	if(engine_int_req & 0x01) switch(1) { case 1: // H2C
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));
		xdma_h2c_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x4, 0, 0));

		xdma_irq_block[0x18 >> 2] = 0x01; // W1C engine_int_req[0:0]
		Status = xdma_h2c_channel[0x44 >> 2];

#if 0
		pr_info("Engine Interrupt H2C, Status=%d\n", Status);
		pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);
#endif

		xdma_h2c_channel[0x04 >> 2] = 0; // Stop

		spin_lock(&self->lock);
		if(list_empty(&self->job_list)) {
			spin_unlock(&self->lock);
			pr_err("self->job_list is empty\n");
			break;
		}

		// move job from job_list to done_list
		done_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
		list_del(&done_entry->node);
		// try pick next job
		next_entry = list_empty(&self->job_list) ? NULL :
			list_first_entry(&self->job_list, struct qvio_buf_entry, node);

		list_add_tail(&done_entry->node, &self->done_list);
		spin_unlock(&self->lock);

		// job done wake up
		wake_up_interruptible(&self->irq_wait);

		// try to do another job
		if(next_entry) {
			xdma_irq_block[0x14 >> 2] = 0x01; // W1S engine_int_req[0:0]
			xdma_h2c_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(next_entry->dsc_adr));
			xdma_h2c_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(next_entry->dsc_adr));
			xdma_h2c_sgdma[0x88 >> 2] = next_entry->dsc_adj;
			xdma_h2c_channel[0x04 >> 2] = BIT(0) | BIT(1); // Run & ie_descriptor_stopped
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}
#endif

static struct qvio_cdev_class __cdev_class;
static const unsigned int __reset_delay = 100;

static void __free(struct kref *ref);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait);
static int __buf_entry_from_sgt(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry);
static int __start_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry);
static int __streamon(struct qvio_video_queue* self);
static int __streamoff(struct qvio_video_queue* self);

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
	.poll = __file_poll,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
};

int qvio_xdma_rd_register(void) {
	int err;

	pr_info("\n");

	err = qvio_cdev_register(&__cdev_class, 0, 255, "xdma_rd");
	if(err) {
		pr_err("qvio_cdev_register() failed\n");
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_xdma_rd_unregister(void) {
	pr_info("\n");

	qvio_cdev_unregister(&__cdev_class);
}

struct qvio_xdma_rd* qvio_xdma_rd_new(void) {
	int err;
	struct qvio_xdma_rd* self = kzalloc(sizeof(struct qvio_xdma_rd), GFP_KERNEL);

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

struct qvio_xdma_rd* qvio_xdma_rd_get(struct qvio_xdma_rd* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __free(struct kref *ref) {
	struct qvio_xdma_rd* self = container_of(ref, struct qvio_xdma_rd, ref);

	// pr_info("\n");

	qvio_video_queue_put(self->video_queue);
	kfree(self);
}

void qvio_xdma_rd_put(struct qvio_xdma_rd* self) {
	if (self)
		kref_put(&self->ref, __free);
}

int qvio_xdma_rd_probe(struct qvio_xdma_rd* self) {
	int err;

	self->video_queue->dev = self->dev;
	self->video_queue->device_id = self->device_id;

	self->desc_pool = dma_pool_create("xdma_rd", self->dev, PAGE_SIZE, 32, 0);
	if(!self->desc_pool) {
		pr_err("dma_pool_create() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev, &__cdev_class);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err1;
	}

	return 0;

err1:
	dma_pool_destroy(self->desc_pool);
err0:
	return err;
}

void qvio_xdma_rd_remove(struct qvio_xdma_rd* self) {
	qvio_cdev_stop(&self->cdev, &__cdev_class);
	dma_pool_destroy(self->desc_pool);
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_xdma_rd* self = filp->private_data;

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
	struct qvio_xdma_rd* self = filp->private_data;

	return qvio_video_queue_file_poll(self->video_queue, filp, wait);
}

static int __buf_entry_from_sgt(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry) {
	int err;
	struct qvio_xdma_rd* xdma_rd = self->parent;
	struct scatterlist* sg;
	int nents;
	size_t buffer_size;
	struct dma_block_t* pDmaBlock;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	dma_addr_t nxt_addr;
	int Nxt_adj;
	ssize_t sg_bytes;
	int i;

	buf_entry->dev = xdma_rd->dev;
	buf_entry->desc_pool = xdma_rd->desc_pool;

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

	pDmaBlock = &buf_entry->desc_blocks[0];
	err = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
	if(err) {
		pr_err("qvio_dma_block_alloc() failed, ret=%d\n", err);
		goto err0;
	}

	pSgdmaDesc = pDmaBlock->cpu_addr;
	Nxt_adj = nents - 1;
	dst_addr = 0xA0000000;

	sg_bytes = 0;
	for (i = 0; i < nents && sg_bytes < buffer_size; i++, sg = sg_next(sg), pSgdmaDesc++, Nxt_adj--) {
		src_addr = sg_dma_address(sg);
		nxt_addr = pDmaBlock->dma_handle + ((u8*)(pSgdmaDesc + 1) - (u8*)pDmaBlock->cpu_addr);

#if 0
		pr_warn("%d, src_addr 0x%llx, src_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d len %d\n",
			i, src_addr, src_addr, nxt_addr, Nxt_adj, sg_dma_len(sg));
#endif

#if 1
		pSgdmaDesc->control = cpu_to_le32(XDMA_DESC_MAGIC | (Nxt_adj << 8));
		pSgdmaDesc->bytes = sg_dma_len(sg);
		pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
		pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
		pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
		pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
		pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
		pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));
#endif

		sg_bytes += sg_dma_len(sg);
		dst_addr += sg_dma_len(sg);
	}

#if 1
	pSgdmaDesc--;
	pSgdmaDesc->control |= cpu_to_le32(XDMA_DESC_STOPPED);
	pSgdmaDesc->next_lo = 0;
	pSgdmaDesc->next_hi = 0;
#endif

	buf_entry->dsc_adr = pDmaBlock->dma_handle;
	buf_entry->dsc_adj = i - 1;
#if 0
	pr_warn("dsc_adr 0x%llx, dsc_adj %u\n", buf_entry->dsc_adr, buf_entry->dsc_adj);
#endif

	dma_sync_single_for_device(buf_entry->dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_FROM_DEVICE);

	return 0;

err0:
	return err;
}

static int __start_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry) {
	struct qvio_xdma_rd* xdma_rd = self->parent;
	uintptr_t irq_block = (uintptr_t)((u64)xdma_rd->reg + xdma_mkaddr(0x2, xdma_rd->channel, 0));
	uintptr_t h2c_channel = (uintptr_t)((u64)xdma_rd->reg + xdma_mkaddr(0x0, xdma_rd->channel, 0));
	uintptr_t h2c_sgdma = (uintptr_t)((u64)xdma_rd->reg + xdma_mkaddr(0x4, xdma_rd->channel, 0));
	u32 value;

#if 1
	value = io_read_reg(irq_block, 0x14);
	io_write_reg(irq_block, 0x14, value | BIT(0)); // W1S engine_int_req[0:0]
	io_write_reg(h2c_sgdma, 0x80, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
	io_write_reg(h2c_sgdma, 0x84, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
	io_write_reg(h2c_sgdma, 0x88, buf_entry->dsc_adj);
	io_write_reg(h2c_channel, 0x04, BIT(0) | BIT(1)); // Run & ie_descriptor_stopped
#endif
	pr_info("XDMA H2C started...\n");

	return 0;
}

static int __streamon(struct qvio_video_queue* self) {
	struct qvio_xdma_rd* xdma_rd = self->parent;
	uintptr_t irq_block = (uintptr_t)((u64)xdma_rd->reg + xdma_mkaddr(0x2, xdma_rd->channel, 0));
	uintptr_t h2c_channel = (uintptr_t)((u64)xdma_rd->reg + xdma_mkaddr(0x0, xdma_rd->channel, 0));
	u32 value;

	// IRQ Block Channel Interrupt Enable Mask
	value = io_read_reg(irq_block, 0x10);
	io_write_reg(irq_block, 0x10, value | BIT(0)); // Enable engine_int_req[0]
	value = io_read_reg(irq_block, 0x18);
	io_write_reg(irq_block, 0x18, value | BIT(0)); // W1C engine_int_req[0]

	io_write_reg(h2c_channel, 0x90, BIT(1)); // im_descriptor_stopped

	return 0;
}

static int __streamoff(struct qvio_video_queue* self) {
	struct qvio_xdma_rd* xdma_rd = self->parent;

	pr_info("TODO\n");

	return 0;
}

irqreturn_t qvio_xdma_rd_irq_handler(int irq, void *dev_id) {
	pr_info("TODO\n");

	return IRQ_NONE;
}
