#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include "xdma_wr.h"
#include "uapi/qvio-l4t.h"
#include "xdma_desc.h"
#include "utils.h"

#if 0
irqreturn_t usr_irq_handler() {
	u32 usr_irq_req, usr_int_pend;
	int i, intr_mask;

	usr_irq_req = io_read_reg(irq_block, 0x40);
	usr_int_pend = io_read_reg(irq_block, 0x48);

	// pr_info("usr_irq_req=0x%X usr_int_pend=0x%X, intr=0x%X\n", usr_irq_req, usr_int_pend, intr);
	for(i = 0;i < 4;i++) {
		intr_mask = 1 << i;

		if(usr_irq_req & intr_mask) {
			pr_info("User Interrupt %d\n", i);
		}
	}
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

int qvio_xdma_wr_register(void) {
	int err;

	pr_info("\n");

	err = qvio_cdev_register(&__cdev_class, 0, 255, "xdma_wr");
	if(err) {
		pr_err("qvio_cdev_register() failed\n");
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_xdma_wr_unregister(void) {
	pr_info("\n");

	qvio_cdev_unregister(&__cdev_class);
}

struct qvio_xdma_wr* qvio_xdma_wr_new(void) {
	int err;
	struct qvio_xdma_wr* self = kzalloc(sizeof(struct qvio_xdma_wr), GFP_KERNEL);

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

struct qvio_xdma_wr* qvio_xdma_wr_get(struct qvio_xdma_wr* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __free(struct kref *ref) {
	struct qvio_xdma_wr* self = container_of(ref, struct qvio_xdma_wr, ref);

	// pr_info("\n");

	qvio_video_queue_put(self->video_queue);
	kfree(self);
}

void qvio_xdma_wr_put(struct qvio_xdma_wr* self) {
	if (self)
		kref_put(&self->ref, __free);
}

int qvio_xdma_wr_probe(struct qvio_xdma_wr* self) {
	int err;

	self->video_queue->dev = self->dev;
	self->video_queue->device_id = self->device_id;

	self->desc_pool = dma_pool_create("xdma_wr", self->dev, PAGE_SIZE, 32, 0);
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

void qvio_xdma_wr_remove(struct qvio_xdma_wr* self) {
	qvio_cdev_stop(&self->cdev, &__cdev_class);
	dma_pool_destroy(self->desc_pool);
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_xdma_wr* self = filp->private_data;

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
	struct qvio_xdma_wr* self = filp->private_data;

	return qvio_video_queue_file_poll(self->video_queue, filp, wait);
}

static int __buf_entry_from_sgt(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry) {
	int err;
	struct qvio_xdma_wr* xdma_wr = self->parent;
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

	buf_entry->dev = xdma_wr->dev;
	buf_entry->desc_pool = xdma_wr->desc_pool;

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

	pSgdmaDesc->control = cpu_to_le32(XDMA_DESC_MAGIC | XDMA_DESC_STOPPED | XDMA_DESC_COMPLETED);

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
	struct qvio_xdma_wr* xdma_wr = self->parent;
	uintptr_t c2h_channel = (uintptr_t)((u64)xdma_wr->reg + xdma_mkaddr(0x1, xdma_wr->channel, 0));
	uintptr_t c2h_sgdma = (uintptr_t)((u64)xdma_wr->reg + xdma_mkaddr(0x5, xdma_wr->channel, 0));
	u32 value;

#if 1
	io_write_reg(c2h_sgdma, 0x80, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
	io_write_reg(c2h_sgdma, 0x84, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
	io_write_reg(c2h_sgdma, 0x88, buf_entry->dsc_adj);
	io_write_reg(c2h_channel, 0x04, BIT(0) | BIT(1) | BIT(2) | BIT(27)); // Run & ie_descriptor_stopped & im_descriptor_completd & disable_writeback
#endif
	pr_info("XDMA C2H started... dsc_adr 0x%llx, dsc_adj %u\n", buf_entry->dsc_adr, buf_entry->dsc_adj);

	return 0;
}

static int __streamon(struct qvio_video_queue* self) {
	struct qvio_xdma_wr* xdma_wr = self->parent;
	uintptr_t irq_block = (uintptr_t)((u64)xdma_wr->reg + xdma_mkaddr(0x2, xdma_wr->channel, 0));
	uintptr_t c2h_channel = (uintptr_t)((u64)xdma_wr->reg + xdma_mkaddr(0x1, xdma_wr->channel, 0));
	u32 value;

	io_write_reg(c2h_channel, 0x90, BIT(1) | BIT(2)); // im_descriptor_stopped & im_descriptor_completd
	io_write_reg(irq_block, 0x10, BIT(1)); // W1S channel_int_enmask[1]

	return 0;
}

static int __streamoff(struct qvio_video_queue* self) {
	struct qvio_xdma_wr* xdma_wr = self->parent;

	pr_info("TODO\n");

	return 0;
}

irqreturn_t qvio_xdma_wr_irq_handler(int irq, void *dev_id) {
	int err;
	struct qvio_xdma_wr* self = dev_id;
	uintptr_t irq_block = (uintptr_t)((u64)self->reg + xdma_mkaddr(0x2, self->channel, 0));
	uintptr_t c2h_channel = (uintptr_t)((u64)self->reg + xdma_mkaddr(0x1, self->channel, 0));
	uintptr_t c2h_sgdma = (uintptr_t)((u64)self->reg + xdma_mkaddr(0x5, self->channel, 0));
	u32 engine_int_req, engine_int_pend;
	u32 compl_descriptor_count;
	u32 Status;
	u32 value;
	struct qvio_buf_entry* buf_entry;

#if 0
	pr_info("XDMA, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	engine_int_req = io_read_reg(irq_block, 0x44);
	// engine_int_pend = io_read_reg(irq_block, 0x4C);
	compl_descriptor_count = io_read_reg(c2h_channel, 0x48);

	// pr_info("engine_int_req=0x%X engine_int_pend=0x%X\n", engine_int_req, engine_int_pend);
	if(! (engine_int_req & BIT(1))) { // C2H engine_int_req[1]
		return IRQ_NONE;
	}

	io_write_reg(irq_block, 0x18, BIT(1)); // W1C channel_int_enmask[1]
	Status = io_read_reg(c2h_channel, 0x44); // engine_int_req
	// pr_info("Engine Interrupt C2H, Status=%d\n", Status);

	io_write_reg(c2h_channel, 0x04, 0); // Stop

	err = qvio_video_queue_done(self->video_queue, &buf_entry);
	if(err) {
		pr_err("qvio_video_queue_done() failed, err=%d\n", err);
		goto err0;
	}

	if(! buf_entry) {
		pr_warn("unexpected value, buf_entry=%llX\n", (int64_t)buf_entry);
		goto err0;
	}

#if 1
	// try to do another job
	io_write_reg(irq_block, 0x14, BIT(1)); // W1S channel_int_enmask[1]
	io_write_reg(c2h_sgdma, 0x80, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
	io_write_reg(c2h_sgdma, 0x84, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
	io_write_reg(c2h_sgdma, 0x88, buf_entry->dsc_adj);
	io_write_reg(c2h_channel, 0x04, BIT(0) | BIT(1) | BIT(2) | BIT(27)); // Run & ie_descriptor_stopped & im_descriptor_completd & disable_writeback
#endif

err0:
	return IRQ_HANDLED;
}

int qvio_xdma_wr_test_case_0(struct qvio_xdma_wr* self, struct dma_block_t* dma_blocks) {
	int err;
	uintptr_t c2h_channel = (uintptr_t)((u64)self->reg + xdma_mkaddr(0x1, self->channel, 0));
	uintptr_t c2h_sgdma = (uintptr_t)((u64)self->reg + xdma_mkaddr(0x5, self->channel, 0));
	dma_addr_t sgdma_desc;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	u8* pDst;
	dma_addr_t nxt_addr;
	int i;

	sgdma_desc = dma_blocks[0].dma_handle;
	pSgdmaDesc = (struct xdma_desc*)dma_blocks[0].cpu_addr;
	src_addr = 0xA0000000;
	dst_addr = dma_blocks[1].dma_handle;
	pDst = dma_blocks[1].cpu_addr;
	nxt_addr = 0;

	for(i = 0;i < 4096;i++) {
		pDst[i] = 0;
	}

	pSgdmaDesc->control = cpu_to_le32(XDMA_DESC_MAGIC | XDMA_DESC_STOPPED | XDMA_DESC_COMPLETED);
	pSgdmaDesc->bytes = cpu_to_le32(4096);
	pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
	pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
	pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
	pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
	pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
	pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));

	dma_sync_single_for_device(self->dev, sgdma_desc, 4096, DMA_TO_DEVICE);

	pr_info("C2H Channel Identifier: 0x%08X\n", io_read_reg(c2h_channel, 0x00));
	pr_info("C2H Channel Control: 0x%08X\n", io_read_reg(c2h_channel, 0x04));
	pr_info("C2H SGDMA Identifier: 0x%08X\n", io_read_reg(c2h_sgdma, 0x00));
	pr_info("C2H Channel Status: 0x%X\n", io_read_reg(c2h_channel, 0x40));
	pr_info("C2H Channel Completed Descriptor Count: %d\n", io_read_reg(c2h_channel, 0x48));

	io_write_reg(c2h_sgdma, 0x80, cpu_to_le32(PCI_DMA_L(sgdma_desc)));
	io_write_reg(c2h_sgdma, 0x84, cpu_to_le32(PCI_DMA_H(sgdma_desc)));
	io_write_reg(c2h_sgdma, 0x88, 0);

	io_write_reg(c2h_channel, 0x04, BIT(0) | BIT(2) | BIT(27)); // Run & ie_descriptor_completed & disable_writeback

	msleep(1000);

	pr_info("C2H Channel Status: 0x%08X\n", io_read_reg(c2h_channel, 0x44));
	pr_info("C2H Channel Completed Descriptor Count: %d\n", io_read_reg(c2h_channel, 0x48));

	io_write_reg(c2h_channel, 0x04, 0);

	dma_sync_single_for_cpu(self->dev, dst_addr, 4096, DMA_FROM_DEVICE);

	return 0;
}
