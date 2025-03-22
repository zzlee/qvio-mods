#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device.h"
#include "device.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/aer.h>
#include <linux/pci.h>
#include <linux/delay.h>

#define DRV_MODULE_NAME "qvio-l4t"

/* Interrupt Status and Control */
#define IE_AP_DONE		BIT(0)
#define IE_AP_READY		BIT(1)

#define ISR_AP_DONE_IRQ		BIT(0)
#define ISR_AP_READY_IRQ		BIT(1)

#define ISR_ALL_IRQ_MASK	\
		(ISR_AP_DONE_IRQ | \
		ISR_AP_READY_IRQ)

static const struct pci_device_id __pci_ids[] = {
	{ PCI_DEVICE(0x12AB, 0xE380), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, __pci_ids);

static void sgt_dump(struct sg_table *sgt, bool full)
{
	int i;
	struct scatterlist *sg = sgt->sgl;
	dma_addr_t dma_addr;

	dma_addr = sg_dma_address(sg);

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u, dma_addr=%p. (PAGE_SIZE=%lu, PAGE_MASK=%lX)\n", sgt, sgt->sgl, sgt->nents,
		sgt->orig_nents, (void*)dma_addr, PAGE_SIZE, PAGE_MASK);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg)) {
		if(! full && i > 8) {
			pr_info("... more pages ...\n");
			break;
		}

		if((sg_dma_len(sg) & ~PAGE_MASK)) {
			pr_err("unexpected dma_size!! %lX\n", (sg_dma_len(sg) & ~PAGE_MASK));
		}

		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n", i, sg,
			sg_page(sg), sg->offset, sg->length, sg_dma_address(sg),
			sg_dma_len(sg));
	}
}

static ssize_t dma_block_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	ssize_t count = PAGE_SIZE;

	memcpy(buf, p, count);

	return count;
}

static ssize_t dma_block_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	count = min(count, PAGE_SIZE);

	memcpy(p, buf, count);

	return count;
}

static ssize_t dma_block_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->dma_block_index);
}

static ssize_t dma_block_index_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	int dma_block_index;

	sscanf(buf, "%d", &dma_block_index);

	if(dma_block_index >= 0 && dma_block_index < ARRAY_SIZE(self->dma_blocks)) {
		self->dma_block_index = dma_block_index;
	}

	return count;
}

static void do_test_case_0(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	int nWidth = 64;
	int nHeight = 32;
	int nStride = nWidth + 16;
	struct desc_item_t* pDescBase;
	struct desc_item_t* pDescItem;
	u64 nOffset;
	u32 nIsIdle;

	pDescBase = self->dma_blocks[0].cpu_addr;
	pDescItem = pDescBase;

	// buffer desc-item
	nOffset = 0;
	pDescItem->nOffsetHigh = ((nOffset >> 32) & 0xFFFFFFFF);
	pDescItem->nOffsetLow = (nOffset & 0xFFFFFFFF);
	pDescItem->nSize = nStride * nHeight;
	pDescItem->nFlags = 0;
	pr_info("(%08X %08X) %08X %08X\n", pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
	pDescItem++;

	// end of desc items
	pDescItem->nOffsetHigh = 0;
	pDescItem->nOffsetLow = 0;
	pDescItem->nSize = 0;
	pDescItem->nFlags = 0;

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, PAGE_SIZE, DMA_BIDIRECTIONAL);

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(pXaximm_test1);
	XAximm_test1_InterruptEnable(pXaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(pXaximm_test1);

	XAximm_test1_Set_pDescItem(pXaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(pXaximm_test1, 1);
	XAximm_test1_Set_pDstPxl(pXaximm_test1, self->dma_blocks[1].dma_handle);
	XAximm_test1_Set_nDstPxlStride(pXaximm_test1, nStride);
	XAximm_test1_Set_nWidth(pXaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(pXaximm_test1, nHeight);

	XAximm_test1_Start(pXaximm_test1);

	return;

err0:
	return;
}

static void do_test_case_1(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	u32 nIsIdle;

	XAximm_test1_InterruptClear(pXaximm_test1, 0x01); // ap_done;

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
}

static void do_test_case_2(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	int nWidth = 64;
	int nHeight = 128;
	int nStride = nWidth + 16;
	struct desc_item_t* pDescBase;
	struct desc_item_t* pDescItem;
	dma_addr_t pDstBase;
	size_t nDstSize = nStride * nHeight;
	int64_t nDstOffset;
	int nDmaBlockIndex = 1;
	int nDescItems = 0;
	u32 nIsIdle;

	pDescBase = self->dma_blocks[0].cpu_addr;
	pDstBase = self->dma_blocks[1].dma_handle;
	pDescItem = pDescBase;

	// buffer desc-item
	do {
		nDstOffset = self->dma_blocks[nDmaBlockIndex].dma_handle - pDstBase;
		pDescItem->nOffsetHigh = ((nDstOffset >> 32) & 0xFFFFFFFF);
		pDescItem->nOffsetLow = (nDstOffset & 0xFFFFFFFF);
		pDescItem->nSize = min(PAGE_SIZE, nDstSize);
		pDescItem->nFlags = 0;
		pr_info("[%d] (%08X %08X) %08X %08X\n", nDmaBlockIndex, pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
		nDstSize -= pDescItem->nSize;
		nDescItems++;
		nDmaBlockIndex++;

		pDescItem++;
	} while(nDstSize > 0);
	pr_warn("nDmaBlockIndex=%u\n", nDmaBlockIndex);

	// end of desc items
	pDescItem->nOffsetHigh = 0;
	pDescItem->nOffsetLow = 0;
	pDescItem->nSize = 0;
	pDescItem->nFlags = 0;

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, PAGE_SIZE, DMA_BIDIRECTIONAL);

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(pXaximm_test1);
	XAximm_test1_InterruptEnable(pXaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(pXaximm_test1);

	XAximm_test1_Set_pDescItem(pXaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(pXaximm_test1, nDescItems);
	XAximm_test1_Set_pDstPxl(pXaximm_test1, pDstBase);
	XAximm_test1_Set_nDstPxlStride(pXaximm_test1, nStride);
	XAximm_test1_Set_nWidth(pXaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(pXaximm_test1, nHeight);

	XAximm_test1_Start(pXaximm_test1);
	return;

err0:
	return;
}

static void do_test_case(struct qvio_device* self, int test_case) {
	switch(test_case) {
	case 0:
		pr_info("+test_case %d\n", test_case);
		do_test_case_0(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 1:
		pr_info("+test_case %d\n", test_case);
		do_test_case_1(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 2:
		pr_info("+test_case %d\n", test_case);
		do_test_case_2(self);
		pr_info("-test_case %d\n", test_case);
		break;

	default:
		pr_warn("unexpected test_case %d\n", test_case);
		break;
	}
}

static ssize_t test_case_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->test_case);
}

static ssize_t test_case_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	int test_case;

	sscanf(buf, "%d", &test_case);
	do_test_case(self, test_case);
	self->test_case = test_case;

	return count;
}

static ssize_t zzlab_ver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	u32 nPlatform = *(u32*)((u8*)self->zzlab_env + 0x14);

	return snprintf(buf, PAGE_SIZE, "0x%08X %c%c%c%c 0x%08X\n",
		*(u32*)((u8*)self->zzlab_env + 0x10),
		(char)((nPlatform >> 24) & 0xFF),
		(char)((nPlatform >> 16) & 0xFF),
		(char)((nPlatform >> 8) & 0xFF),
		(char)((nPlatform >> 0) & 0xFF),
		*(u32*)((u8*)self->zzlab_env + 0x18));
}

static ssize_t zzlab_ticks_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
		*(u32*)((u8*)self->zzlab_env + 0x1C));
}

static DEVICE_ATTR(dma_block, 0644, dma_block_show, dma_block_store);
static DEVICE_ATTR(dma_block_index, 0644, dma_block_index_show, dma_block_index_store);
static DEVICE_ATTR(test_case, 0644, test_case_show, test_case_store);
static DEVICE_ATTR(zzlab_ver, 0444, zzlab_ver_show, NULL);
static DEVICE_ATTR(zzlab_ticks, 0444, zzlab_ticks_show, NULL);

static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait);
static ssize_t __file_read(struct file *filp, char *buf, size_t size, loff_t *f_pos);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static long __file_ioctl_g_ticks(struct file * filp, unsigned long arg);
static long __file_ioctl_s_fmt(struct file * filp, unsigned long arg);
static long __file_ioctl_g_fmt(struct file * filp, unsigned long arg);
static long __file_ioctl_qbuf(struct file * filp, unsigned long arg);
static long __file_ioctl_dqbuf(struct file * filp, unsigned long arg);
static long __file_ioctl_streamon(struct file * filp, unsigned long arg);
static long __file_ioctl_streamoff(struct file * filp, unsigned long arg);

static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait) {
	struct qvio_device* self = filp->private_data;

	poll_wait(filp, &self->irq_wait, wait);

	if (self->irq_event_count != atomic_read(&self->irq_event))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static ssize_t __file_read(struct file *filp, char *buf, size_t size, loff_t *f_pos) {
	struct qvio_device* self = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret = 0;
	u32 event_count;
	u32 event_data;

	if (size != sizeof(u32))
		return -EINVAL;

	add_wait_queue(&self->irq_wait, &wait);

	do {
		set_current_state(TASK_INTERRUPTIBLE);

		event_count = atomic_read(&self->irq_event);
		if (event_count != self->irq_event_count) {
			__set_current_state(TASK_RUNNING);
			event_data = atomic_read(&self->irq_event_data);
			if (copy_to_user(buf, &event_data, size)) {
				ret = -EFAULT;
			} else {
				self->irq_event_count = event_count;
				ret = size;
			}
			break;
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		schedule();
	} while (1);

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&self->irq_wait, &wait);

	return ret;
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;

	switch(cmd) {
	case QVIO_IOC_G_TICKS:
		ret = __file_ioctl_g_ticks(filp, arg);
		break;

	case QVIO_IOC_S_FMT:
		ret = __file_ioctl_s_fmt(filp, arg);
		break;

	case QVIO_IOC_G_FMT:
		ret = __file_ioctl_g_fmt(filp, arg);
		break;

	case QVIO_IOC_QBUF:
		ret = __file_ioctl_qbuf(filp, arg);
		break;

	case QVIO_IOC_DQBUF:
		ret = __file_ioctl_dqbuf(filp, arg);
		break;

	case QVIO_IOC_STREAMON:
		ret = __file_ioctl_streamon(filp, arg);
		break;

	case QVIO_IOC_STREAMOFF:
		ret = __file_ioctl_streamoff(filp, arg);
		break;

	default:
		pr_err("unexpected, cmd=%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long __file_ioctl_g_ticks(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_g_ticks args;

	args.ticks = *(u32*)((u8*)self->zzlab_env + 0x1C);

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

err0:
	return ret;
}

static long __file_ioctl_s_fmt(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_g_fmt args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	self->width = args.width;
	self->height = args.height;
	self->fmt = args.fmt;

	pr_info("%dx%d 0x%08X\n", self->width, self->height, self->fmt);

err0:
	return ret;
}

static long __file_ioctl_g_fmt(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_g_fmt args;

	args.width = self->width;
	args.height = self->height;
	args.fmt = self->fmt;

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

err0:
	return ret;
}

static long __file_ioctl_qbuf_userptr(struct file * filp, struct qvio_qbuf* qbuf) {
	struct qvio_device* self = filp->private_data;
	long ret;
	int size;
	void* vaddr;
	unsigned long start;
	unsigned long length;
	unsigned long first, last;
	unsigned long nr;
	struct frame_vector *vec;
	unsigned int flags_vec = FOLL_FORCE | FOLL_WRITE;
	int nr_pages;
	struct page **pages;
	struct sg_table sgt;
	enum dma_data_direction dma_dir = DMA_FROM_DEVICE;
	struct qvio_buf_entry* buf_entry;
	struct scatterlist *sg;
	unsigned long flags;
	int i;
	size_t buf_size;
	struct desc_item_t* pDescItem;
	int64_t nDstOffset;

	size = (int)qbuf->stride[0] * self->height;
	vaddr = (void*)qbuf->u.userptr;

	// pr_info("size=%d vaddr=%llX\n", size, (int64_t)vaddr);

	start = (unsigned long)vaddr;
	length = size;
	first = start >> PAGE_SHIFT;
	last = (start + length - 1) >> PAGE_SHIFT;
	nr = last - first + 1;
	vec = frame_vector_create(nr);
	if(! vec) {
		pr_err("frame_vector_create() failed\n");
		goto err0;
	}

	// pr_info("start=%lu, nr=%lu\n", start, nr);

	ret = get_vaddr_frames(start & PAGE_MASK, nr, flags_vec, vec);
	if (ret < 0) {
		pr_err("get_vaddr_frames() failed, err=%ld\n", ret);
		goto err1;
	}

	nr_pages = frame_vector_count(vec);
	// pr_info("nr_pages=%d\n", nr_pages);

	ret = frame_vector_to_pages(vec);
	if (ret < 0) {
		pr_err("frame_vector_to_pages() failed, err=%ld\n", ret);
		goto err2;
	}

	pages = frame_vector_pages(vec);
	ret = sg_alloc_table_from_pages(&sgt, pages, nr_pages, 0, size, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed, err=%ld\n", ret);
		goto err2;
	}

	ret = dma_map_sg(self->dev, sgt.sgl, sgt.nents, dma_dir);
	if (ret <= 0) {
		pr_err("dma_map_sg() failed, err=%ld\n", ret);
		ret = -EPERM;
		goto err3;
	}

	// sgt_dump(&sgt, true);

#if 0
	dma_unmap_sg(self->dev, sgt.sgl, sgt.nents, dma_dir);
	sg_free_table(&sgt);
	put_vaddr_frames(vec);
	frame_vector_destroy(vec);

	return -EINVAL;
#endif

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = -ENOMEM;
		goto err4;
	}

	buf_entry->desc_pool = self->desc_pool;
	buf_size = self->width * self->height; // TODO: must calc by self->fmt
	ret = qvio_dma_block_alloc(&buf_entry->desc_blocks[0], buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
	if(ret) {
		pr_err("qvio_dma_block_alloc() failed, ret=%ld\n", ret);
		goto err5;
	}

	if((sgt.orig_nents << DESC_BITSHIFT) > PAGE_SIZE) {
		pr_err("unexpected!! sgt.orig_nents=%d\n", (int)sgt.orig_nents);
	}

	pDescItem = buf_entry->desc_blocks[0].cpu_addr;
	buf_entry->desc_dma_handle = buf_entry->desc_blocks[0].dma_handle;

	sg = sgt.sgl;
	for (i = 0; i < sgt.orig_nents && buf_size > 0; i++, sg = sg_next(sg)) {
		if(i == 0) {
			buf_entry->dst_dma_handle = sg_dma_address(sg);
		}

		nDstOffset = sg_dma_address(sg) - buf_entry->dst_dma_handle;
		pDescItem->nOffsetHigh = ((nDstOffset >> 32) & 0xFFFFFFFF);
		pDescItem->nOffsetLow = (nDstOffset & 0xFFFFFFFF);
		pDescItem->nSize = min(buf_size, (size_t)sg_dma_len(sg));
		pDescItem->nFlags = 0;
		// pr_info("%d: (%08X %08X) %08X %08X, buf_size=%u\n", i, pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags, buf_size);

		buf_entry->desc_items++;
		buf_size -= pDescItem->nSize;

		pDescItem++;
	}

	dma_sync_single_for_device(self->dev, buf_entry->desc_blocks[0].dma_handle, PAGE_SIZE, DMA_BIDIRECTIONAL);

	buf_entry->qbuf = *qbuf;
	buf_entry->dev = self->dev;
	buf_entry->sgt_userptr = sgt;
	buf_entry->dma_dir = dma_dir;

	spin_lock_irqsave(&self->job_list_lock, flags);
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->job_list_lock, flags);

	return 0;

err5:
	qvio_buf_entry_put(buf_entry);
err4:
	dma_unmap_sg(self->dev, sgt.sgl, sgt.nents, dma_dir);
err3:
	sg_free_table(&sgt);
err2:
	put_vaddr_frames(vec);
err1:
	frame_vector_destroy(vec);
err0:
	return ret;
}

static long __file_ioctl_qbuf_dmabuf(struct file * filp, struct qvio_qbuf* qbuf) {
	struct qvio_device* self = filp->private_data;
	long ret;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct qvio_buf_entry* buf_entry;
	struct scatterlist *sg;
	enum dma_data_direction dma_dir = DMA_FROM_DEVICE;
	unsigned long flags;
	int i;
	size_t buf_size;
	struct desc_item_t* pDescItem;
	int64_t nDstOffset;

	dmabuf = dma_buf_get(qbuf->u.fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get() failed, dmabuf=%p\n", dmabuf);

		ret = -EINVAL;
		goto err0;
	}

	// pr_info("dmabuf->size=%d\n", (int)dmabuf->size);

	attach = dma_buf_attach(dmabuf, self->dev);
	if (IS_ERR(attach)) {
		pr_err("dma_buf_attach() failed, attach=%p\n", attach);

		ret = -EINVAL;
		goto err1;
	}

	// pr_info("attach=%p\n", attach);

	ret = dma_buf_begin_cpu_access(dmabuf, dma_dir);
	if (IS_ERR(attach)) {
		pr_err("dma_buf_begin_cpu_access() failed, attach=%p\n", attach);
		goto err2;
	}

	sgt = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR(sgt)) {
		pr_err("dma_buf_map_attachment() failed, sgt=%p\n", sgt);

		ret = -EINVAL;
		goto err3;
	}

	// sgt_dump(sgt, true);

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = -ENOMEM;
		goto err4;
	}

	buf_entry->desc_pool = self->desc_pool;
	buf_size = self->width * self->height; // TODO: must calc by self->fmt
	ret = qvio_dma_block_alloc(&buf_entry->desc_blocks[0], buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
	if(ret) {
		pr_err("qvio_dma_block_alloc() failed, ret=%ld\n", ret);
		goto err5;
	}

	if((sgt->orig_nents << DESC_BITSHIFT) > PAGE_SIZE) {
		pr_err("unexpected!! sgt->orig_nents=%d\n", (int)sgt->orig_nents);
	}

	pDescItem = buf_entry->desc_blocks[0].cpu_addr;
	buf_entry->desc_dma_handle = buf_entry->desc_blocks[0].dma_handle;

	sg = sgt->sgl;
	for (i = 0; i < sgt->orig_nents && buf_size > 0; i++, sg = sg_next(sg)) {
		if(i == 0) {
			buf_entry->dst_dma_handle = sg_dma_address(sg);
		}

		nDstOffset = sg_dma_address(sg) - buf_entry->dst_dma_handle;
		pDescItem->nOffsetHigh = ((nDstOffset >> 32) & 0xFFFFFFFF);
		pDescItem->nOffsetLow = (nDstOffset & 0xFFFFFFFF);
		pDescItem->nSize = min(buf_size, (size_t)sg_dma_len(sg));
		pDescItem->nFlags = 0;
		// pr_info("%d: (%08X %08X) %08X %08X, buf_size=%u\n", i, pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags, buf_size);

		buf_entry->desc_items++;
		buf_size -= pDescItem->nSize;

		pDescItem++;
	}

	dma_sync_single_for_device(self->dev, buf_entry->desc_blocks[0].dma_handle, PAGE_SIZE, DMA_BIDIRECTIONAL);

	buf_entry->qbuf = *qbuf;
	buf_entry->dmabuf = dmabuf;
	buf_entry->attach = attach;
	buf_entry->sgt = sgt;
	buf_entry->dma_dir = dma_dir;

	spin_lock_irqsave(&self->job_list_lock, flags);
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->job_list_lock, flags);

	return 0;

err5:
	qvio_buf_entry_put(buf_entry);
err4:
	dma_buf_unmap_attachment(attach, sgt, dma_dir);
err3:
	dma_buf_end_cpu_access(dmabuf, dma_dir);
err2:
	dma_buf_detach(dmabuf, attach);
err1:
	dma_buf_put(dmabuf);
err0:
	return ret;
}

static long __file_ioctl_qbuf(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_qbuf qbuf;

	ret = copy_from_user(&qbuf, (void __user *)arg, sizeof(qbuf));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	switch(qbuf.buf_type) {
	case 1:
		ret = __file_ioctl_qbuf_userptr(filp, &qbuf);
		break;

	case 2:
		ret = __file_ioctl_qbuf_dmabuf(filp, &qbuf);
		break;

	default:
		pr_err("unexpected value, qbuf.buf_type=%d\n", qbuf.buf_type);
		ret = -EINVAL;
		break;
	}

	return ret;

err0:
	return ret;
}

static long __file_ioctl_dqbuf(struct file * filp, unsigned long arg) {
	struct qvio_device* self = filp->private_data;
	long ret;
	unsigned long flags;
	struct qvio_buf_entry* buf_entry;
	struct qvio_dqbuf args;

	if(list_empty(&self->done_list)) {
		ret = -EAGAIN;
		goto err0;
	}

	spin_lock_irqsave(&self->done_list_lock, flags);
	buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
	list_del(&buf_entry->node);
	spin_unlock_irqrestore(&self->done_list_lock, flags);

	// pr_info("buf_entry->qbuf.cookie=0x%llX\n", (int64_t)buf_entry->qbuf.cookie);
	args.cookie = buf_entry->qbuf.cookie;

	qvio_buf_entry_put(buf_entry);

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamon(struct file * filp, unsigned long arg) {
	struct qvio_device* self = filp->private_data;
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	long ret;
	struct qvio_buf_entry* buf_entry;
	u32 value;

	if(list_empty(&self->job_list)) {
		pr_warn("job list not empty!!\n");
		ret = -EPERM;
		goto err0;
	}

	buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);

	pr_info("reset IP...\n"); // [x, aximm_test1, x, x]
	value = *(u32*)((u8*)self->zzlab_env + 0x24);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x4; // 0100
	msleep(100);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x4; // 0100

	XAximm_test1_InterruptClear(pXaximm_test1, ISR_ALL_IRQ_MASK);
	XAximm_test1_DisableAutoRestart(pXaximm_test1);
	XAximm_test1_InterruptEnable(pXaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(pXaximm_test1);

	pr_info("------- %llX %d %llX %d\n",
		(int64_t)buf_entry->desc_dma_handle,
		buf_entry->desc_items,
		(int64_t)buf_entry->dst_dma_handle,
		buf_entry->qbuf.stride[0]);

	XAximm_test1_Set_pDescItem(pXaximm_test1, buf_entry->desc_dma_handle);
	XAximm_test1_Set_nDescItemCount(pXaximm_test1, buf_entry->desc_items);
	XAximm_test1_Set_pDstPxl(pXaximm_test1, buf_entry->dst_dma_handle);
	XAximm_test1_Set_nDstPxlStride(pXaximm_test1, buf_entry->qbuf.stride[0]);
	XAximm_test1_Set_nWidth(pXaximm_test1, self->width);
	XAximm_test1_Set_nHeight(pXaximm_test1, self->height);

	XAximm_test1_Start(pXaximm_test1);
	pr_info("XAximm_test1_Start()...\n");

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamoff(struct file * filp, unsigned long arg) {
	struct qvio_device* self = filp->private_data;
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	int i;
	u32 nIsIdle;
	struct qvio_buf_entry* buf_entry;

	XAximm_test1_InterruptGlobalDisable(pXaximm_test1);

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
		pr_info("%d: XAximm_test1_IsIdle()=%u\n", i, nIsIdle);
		if(nIsIdle)
			break;
		msleep(100);
	}

	if(! nIsIdle) {
		pr_err("nIsIdle=%u\n", nIsIdle);
	}

	XAximm_test1_InterruptClear(pXaximm_test1, ISR_ALL_IRQ_MASK);

	if(! list_empty(&self->job_list)) {
		pr_warn("job list is not empty!!\n");

		while(! list_empty(&self->job_list)) {
			buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}

	if(! list_empty(&self->done_list)) {
		pr_warn("done list is not empty!!\n");

		while(! list_empty(&self->done_list)) {
			buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}

	self->done_jobs = 0;
	init_waitqueue_head(&self->irq_wait);
	atomic_set(&self->irq_event, 0);
	self->irq_event_count = atomic_read(&self->irq_event);
	atomic_set(&self->irq_event_data, 0);

	return 0;
}

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
	.read = __file_read,
	.poll = __file_poll,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
};

static int map_single_bar(struct qvio_device *self, int idx)
{
	struct pci_dev *pdev = self->pci_dev;
	resource_size_t bar_start;
	resource_size_t bar_len;
	resource_size_t map_len;

	bar_start = pci_resource_start(pdev, idx);
	bar_len = pci_resource_len(pdev, idx);
	map_len = bar_len;

	self->bar[idx] = NULL;

	/* do not map BARs with length 0. Note that start MAY be 0! */
	if (!bar_len) {
		//pr_info("BAR #%d is not present - skipping\n", idx);
		return 0;
	}

	/* BAR size exceeds maximum desired mapping? */
	if (bar_len > INT_MAX) {
		pr_info("Limit BAR %d mapping from %llu to %d bytes\n", idx,
			(u64)bar_len, INT_MAX);
		map_len = (resource_size_t)INT_MAX;
	}
	/*
	 * map the full device memory or IO region into kernel virtual
	 * address space
	 */
	pr_info("BAR%d: %llu bytes to be mapped.\n", idx, (u64)map_len);
	self->bar[idx] = pci_iomap(pdev, idx, map_len);

	if (!self->bar[idx]) {
		pr_info("Could not map BAR %d.\n", idx);
		return -1;
	}

	pr_info("BAR%d at 0x%llx mapped at 0x%p, length=%llu(/%llu)\n", idx,
		(u64)bar_start, self->bar[idx], (u64)map_len, (u64)bar_len);

	return (int)map_len;
}

static int map_bars(struct qvio_device* self) {
	int err;
	int i;

	for (i = 0; i < 2; i++) {
		int bar_len;

		bar_len = map_single_bar(self, i);
		if (bar_len == 0) {
			continue;
		} else if (bar_len < 0) {
			err = -EINVAL;
			goto err0;
		}
	}

	return 0;

err0:
	return err;
}

static void unmap_bars(struct qvio_device *self)
{
	int i;
	struct pci_dev *pdev = self->pci_dev;

	for (i = 0; i < 6; i++) {
		/* is this BAR mapped? */
		if (self->bar[i]) {
			/* unmap BAR */
			pci_iounmap(pdev, self->bar[i]);
			/* mark as unmapped */
			self->bar[i] = NULL;
		}
	}
}

#if KERNEL_VERSION(3, 5, 0) <= LINUX_VERSION_CODE
static void pci_enable_capability(struct pci_dev *pdev, int cap)
{
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, cap);
}
#else
static void pci_enable_capability(struct pci_dev *pdev, int cap)
{
	u16 v;
	int pos;

	pos = pci_pcie_cap(pdev);
	if (pos > 0) {
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &v);
		v |= cap;
		pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, v);
	}
}
#endif

static void pci_check_intr_pend(struct pci_dev *pdev)
{
	u16 v;

	pci_read_config_word(pdev, PCI_STATUS, &v);
	if (v & PCI_STATUS_INTERRUPT) {
		pr_info("%s PCI STATUS Interrupt pending 0x%x.\n",
			dev_name(&pdev->dev), v);
		pci_write_config_word(pdev, PCI_STATUS, PCI_STATUS_INTERRUPT);
	}
}

static void pci_keep_intx_enabled(struct pci_dev *pdev)
{
	/* workaround to a h/w bug:
	 * when msix/msi become unavaile, default to legacy.
	 * However the legacy enable was not checked.
	 * If the legacy was disabled, no ack then everything stuck
	 */
	u16 pcmd, pcmd_new;

	pci_read_config_word(pdev, PCI_COMMAND, &pcmd);
	pcmd_new = pcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (pcmd_new != pcmd) {
		pr_info("%s: clear INTX_DISABLE, 0x%x -> 0x%x.\n",
			dev_name(&pdev->dev), pcmd, pcmd_new);
		pci_write_config_word(pdev, PCI_COMMAND, pcmd_new);
	}
}

static int request_regions(struct qvio_device *self)
{
	int rv;
	struct pci_dev *pdev = self->pci_dev;

	if (!self) {
		pr_err("Invalid self\n");
		return -EINVAL;
	}

	if (!pdev) {
		pr_err("Invalid pdev\n");
		return -EINVAL;
	}

	pr_info("pci_request_regions()\n");
	rv = pci_request_regions(pdev, DRV_MODULE_NAME);
	/* could not request all regions? */
	if (rv) {
		pr_info("pci_request_regions() = %d, device in use?\n", rv);
		/* assume device is in use so do not disable it later */
		self->regions_in_use = 1;
	} else {
		self->got_regions = 1;
	}

	return rv;
}

static void release_regions(struct qvio_device *self, struct pci_dev* pdev) {
	if (self->got_regions) {
		pr_info("pci_release_regions 0x%p.\n", pdev);
		pci_release_regions(pdev);
	}

	if (!self->regions_in_use) {
		pr_info("pci_disable_device 0x%p.\n", pdev);
		pci_disable_device(pdev);
	}
}

static int set_dma_mask(struct pci_dev *pdev)
{
	if (!pdev) {
		pr_err("Invalid pdev\n");
		return -EINVAL;
	}

	pr_info("sizeof(dma_addr_t) == %ld\n", sizeof(dma_addr_t));
	/* 64-bit addressing capability for XDMA? */
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		/* query for DMA transfer */
		/* @see Documentation/DMA-mapping.txt */
		pr_info("dma_set_mask()\n");
		/* use 64-bit DMA */
		pr_info("Using a 64-bit DMA mask.\n");
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	} else if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		pr_info("Could not set 64-bit DMA mask.\n");
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		/* use 32-bit DMA */
		pr_info("Using a 32-bit DMA mask.\n");
	} else {
		pr_info("No suitable DMA possible.\n");
		return -EINVAL;
	}

	return 0;
}

#ifndef arch_msi_check_device
static int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

/* type = PCI_CAP_ID_MSI or PCI_CAP_ID_MSIX */
static int msi_msix_capable(struct pci_dev *dev, int type)
{
	struct pci_bus *bus;
	int ret;

	if (!dev || dev->no_msi)
		return 0;

	for (bus = dev->bus; bus; bus = bus->parent)
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
			return 0;

	ret = arch_msi_check_device(dev, 1, type);
	if (ret)
		return 0;

	if (!pci_find_capability(dev, type))
		return 0;

	return 1;
}

static irqreturn_t __irq_handler(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;

	pr_info("IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_0(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("AXIMM_TEST1, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test1_InterruptGetStatus(pXaximm_test1);
	if(! (status & ISR_ALL_IRQ_MASK))
		return IRQ_NONE;

	XAximm_test1_InterruptClear(pXaximm_test1, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
		if(list_empty(&self->job_list)) {
			pr_err("self->job_list is empty\n");
			break;
		}

		// move job from job_list to done_list
		spin_lock(&self->job_list_lock);
		done_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
		list_del(&done_entry->node);
		// try pick next job
		next_entry = list_empty(&self->job_list) ? NULL :
			list_first_entry(&self->job_list, struct qvio_buf_entry, node);
		spin_unlock(&self->job_list_lock);

		spin_lock(&self->done_list_lock);
		list_add_tail(&done_entry->node, &self->done_list);
		spin_unlock(&self->done_list_lock);

		// job done wake up
		atomic_set(&self->irq_event_data, self->done_jobs++);
		atomic_inc(&self->irq_event);
		wake_up_interruptible(&self->irq_wait);

		// try to do another job
		if(next_entry) {
			XAximm_test1_Set_pDescItem(pXaximm_test1, next_entry->desc_dma_handle);
			XAximm_test1_Set_nDescItemCount(pXaximm_test1, next_entry->desc_items);
			XAximm_test1_Set_pDstPxl(pXaximm_test1, next_entry->dst_dma_handle);
			XAximm_test1_Set_nDstPxlStride(pXaximm_test1, next_entry->qbuf.stride[0]);
			XAximm_test1_Set_nWidth(pXaximm_test1, self->width);
			XAximm_test1_Set_nHeight(pXaximm_test1, self->height);

			XAximm_test1_Start(pXaximm_test1);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_1(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;

	pr_info("TPG, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;

	return IRQ_HANDLED;
}

static int enable_msi_msix(struct qvio_device* self, struct pci_dev* pdev) {
	int err;
	int msi_vec_count;

	if(msi_msix_capable(pdev, PCI_CAP_ID_MSIX)) {
		pr_err("unexpected, PCI_CAP_ID_MSIX\n");
		err = -EINVAL;
		goto err0;
	}

	if(msi_msix_capable(pdev, PCI_CAP_ID_MSI)) {
		pci_keep_intx_enabled(pdev);

		msi_vec_count = pci_msi_vec_count(pdev);
		pr_info("pci_msi_vec_count() = %d\n", msi_vec_count);

		err = pci_alloc_irq_vectors_affinity(pdev, msi_vec_count, msi_vec_count, PCI_IRQ_MSI, NULL);
		if(err < 0) {
			pr_err("pci_alloc_irq_vectors_affinity() failed, err=%d\n", err);
			goto err0;
		}

		pr_info("pci_alloc_irq_vectors_affinity() succeeded!\n");
		self->msi_enabled = 1;
	}

	if(! (self->msi_enabled || self->msix_enabled)) {
		pr_err("unexpected, msi_enabled=%d, msix_enabled=%d\n", self->msi_enabled, self->msix_enabled);
		err = -EINVAL;
		goto err0;
	}

	return 0;

err0:
	return err;
}

static void disable_msi_msix(struct qvio_device* self, struct pci_dev* pdev) {
	if (self->msix_enabled) {
		pci_disable_msix(pdev);
		self->msix_enabled = 0;
	} else if (self->msi_enabled) {
		pci_disable_msi(pdev);
		self->msi_enabled = 0;
	}
}

static void free_irqs(struct qvio_device* self) {
	int i;

	for(i = 0;i < ARRAY_SIZE(self->irq_lines);i++) {
		if(self->irq_lines[i]) {
			free_irq(self->irq_lines[i], self);
		}
	}
}

static int irq_setup(struct qvio_device* self, struct pci_dev* pdev) {
	int err;
	int i;
	u32 vector;
	irqreturn_t (*irq_handler)(int, void *);

	if(self->msi_enabled) {
		for(i = 0;i < ARRAY_SIZE(self->irq_lines);i++) {
			vector = pci_irq_vector(pdev, i);
			if(vector == -EINVAL) {
				err = -EINVAL;
				pr_err("pci_irq_vector() failed\n");
				goto err0;
			}

			switch(i) {
			case 0:
				irq_handler = __irq_handler_0;
				break;

			case 1:
				irq_handler = __irq_handler_1;
				break;

			default:
				irq_handler = __irq_handler;
				break;
			}

			err = request_irq(vector, irq_handler, 0, DRV_MODULE_NAME, self);
			if(err) {
				pr_err("%d: request_irq(%u) failed, err=%d\n", i, vector, err);
				goto err0;
			}

			self->irq_lines[i] = vector;
		}
	}

	return 0;

err0:
	free_irqs(self);

	return err;
}

static void dma_blocks_free(struct qvio_device* self) {
	int i;

	for(i = 0;i < ARRAY_SIZE(self->dma_blocks);i++) {
		if(! self->dma_blocks[i].dma_handle)
			continue;

		qvio_dma_block_free(&self->dma_blocks[i], self->desc_pool);
	}
}

static int dma_blocks_alloc(struct qvio_device* self) {
	int err;
	int i;

	for(i = 0;i < ARRAY_SIZE(self->dma_blocks);i++) {
		err = qvio_dma_block_alloc(&self->dma_blocks[i], self->desc_pool, GFP_KERNEL | GFP_DMA);
		if(err) {
			pr_err("qvio_dma_block_alloc() failed, err=%d\n", err);
			goto err0;
		}

		pr_info("dma_blocks[%d]: cpu_addr=%llx dma_handle=%llx", i,
			(u64)self->dma_blocks[i].cpu_addr, (u64)self->dma_blocks[i].dma_handle);
	}

	return 0;

err0:
	dma_blocks_free(self);

	return err;
}

static int create_dev_attrs(struct device* dev) {
	int err;

	err = device_create_file(dev, &dev_attr_dma_block);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err0;
	}

	err = device_create_file(dev, &dev_attr_dma_block_index);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err1;
	}

	err = device_create_file(dev, &dev_attr_test_case);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err2;
	}

	err = device_create_file(dev, &dev_attr_zzlab_ver);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err3;
	}

	err = device_create_file(dev, &dev_attr_zzlab_ticks);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err4;
	}

	return 0;

err4:
	device_remove_file(dev, &dev_attr_zzlab_ver);
err3:
	device_remove_file(dev, &dev_attr_test_case);
err2:
	device_remove_file(dev, &dev_attr_dma_block_index);
err1:
	device_remove_file(dev, &dev_attr_dma_block);
err0:
	return err;
}

static void remove_dev_attrs(struct device* dev) {
	device_remove_file(dev, &dev_attr_zzlab_ticks);
	device_remove_file(dev, &dev_attr_zzlab_ver);
	device_remove_file(dev, &dev_attr_test_case);
	device_remove_file(dev, &dev_attr_dma_block_index);
	device_remove_file(dev, &dev_attr_dma_block);
}

static void test_vmalloc_user(struct device* dev) {
	int ret;
	int size;
	void* vaddr;
	unsigned long nr_pages;
	struct page **pages;
	unsigned long i;
	struct sg_table sgt;

	size = 4096*2160*2;
	vaddr = vmalloc_user(size);
	if(! vaddr) {
		pr_err("vmalloc_user() failed\n");
		goto err0;
	}
	pr_info("vaddr=%llX\n", (int64_t)vaddr);

	nr_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	pr_info("nr_pages=%lu\n", nr_pages);

	pages = kmalloc(sizeof(struct page *) * nr_pages, GFP_KERNEL);
	if (!pages) {
		pr_err("kmalloc() failed\n");
		goto err1;
	}

	for (i = 0; i < nr_pages; i++) {
		pages[i] = vmalloc_to_page(vaddr + (i << PAGE_SHIFT));
		if (!pages[i]) {
			pr_err("vmalloc_to_page() failed\n");
			goto err2;
		}
	}

	ret = sg_alloc_table_from_pages(&sgt, pages, nr_pages, 0, size, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed, err=%d\n", ret);
		goto err2;
	}

	ret = dma_map_sg(dev, sgt.sgl, sgt.nents, DMA_FROM_DEVICE);
	if (ret <= 0) {
		pr_err("dma_map_sg() failed, err=%d\n", ret);
		goto err3;
	}

	sgt_dump(&sgt, true);

// err4:
	dma_unmap_sg(dev, sgt.sgl, sgt.nents, DMA_FROM_DEVICE);
err3:
	sg_free_table(&sgt);
err2:
	kfree(pages);
err1:
	vfree(vaddr);
err0:
	return;
}

static int __pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
	int err = 0;
	struct qvio_device* self;

	pr_info("%04X:%04X (%04X:%04X)\n", (int)pdev->vendor, (int)pdev->device,
		(int)pdev->subsystem_vendor, (int)pdev->subsystem_device);

	test_vmalloc_user(&pdev->dev);

	self = qvio_device_new();
	if(! self) {
		pr_err("qvio_device_new() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	self->dev = &pdev->dev;
	self->pci_dev = pdev;
	dev_set_drvdata(&pdev->dev, self);
	self->device_id = (((int)pdev->subsystem_vendor & 0xFFFF) << 16) |
		((int)pdev->subsystem_device & 0xFFFF);

	pr_info("self->device_id=%08X\n", self->device_id);

	err = pci_enable_device(pdev);
	if (err) {
		pr_err("pci_enable_device() failed, err=%d\n", err);
		goto err0;
	}

	/* keep INTx enabled */
	pci_check_intr_pend(pdev);

	/* enable relaxed ordering */
	pci_enable_capability(pdev, PCI_EXP_DEVCTL_RELAX_EN);

	/* enable extended tag */
	pci_enable_capability(pdev, PCI_EXP_DEVCTL_EXT_TAG);

	/* force MRRS to be 512 */
	err = pcie_set_readrq(pdev, 512);
	if (err)
		pr_info("device %s, error set PCI_EXP_DEVCTL_READRQ: %d.\n",
			dev_name(&pdev->dev), err);

	/* enable bus master capability */
	pci_set_master(pdev);

	err = request_regions(self);
	if(err) {
		pr_err("request_regions() failed, err=%d\n", err);
		goto err1;
	}

	err = map_bars(self);
	if(err) {
		pr_err("map_bars() failed, err=%d\n", err);
		goto err2;
	}

	err = set_dma_mask(pdev);
	if(err) {
		pr_err("set_dma_mask() failed, err=%d\n", err);
		goto err3;
	}

	err = enable_msi_msix(self, pdev);
	if(err) {
		pr_err("enable_msi_msix() failed, err=%d\n", err);
		goto err3;
	}

	err = irq_setup(self, pdev);
	if(err) {
		pr_err("irq_setup() failed, err=%d\n", err);
		goto err4;
	}

	self->zzlab_env = (void __iomem *)(self->bar[0] + 0x00000);
	pr_info("zzlab_env = %p\n", self->zzlab_env);

	pr_info("reset IPs...\n"); // [z_frmbuf_writer, aximm_test1, pcie_intr, tpg]
	*(u32*)((u8*)self->zzlab_env + 0x24) = 0x2; // 0010
	msleep(100);
	*(u32*)((u8*)self->zzlab_env + 0x24) = 0xD; // 1101

	self->xaximm_test1.Control_BaseAddress = (u64)self->bar[0] + 0x20000;
	self->xaximm_test1.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test1 = %p\n", (void*)self->xaximm_test1.Control_BaseAddress);

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err5;
	}

	self->desc_pool = dma_pool_create("desc_pool", &pdev->dev, PAGE_SIZE, DESC_SIZE, 0);
	if(err) {
		pr_err("dma_pool_create() failed, err=%d\n", err);
		goto err6;
	}

	err = dma_blocks_alloc(self);
	if(err) {
		pr_err("dma_blocks_alloc() failed, err=%d\n", err);
		goto err7;
	}

	err = create_dev_attrs(&pdev->dev);
	if (err) {
		pr_err("create_dev_attrs() failed, err=%d\n", err);
		goto err8;
	}

	return 0;

err8:
	dma_blocks_free(self);
err7:
	qvio_cdev_stop(&self->cdev);
err6:
	dma_pool_destroy(self->desc_pool);
err5:
	free_irqs(self);
err4:
	disable_msi_msix(self, pdev);
err3:
	unmap_bars(self);
err2:
	release_regions(self, pdev);
err1:
	qvio_device_put(self);
err0:
	return err;
}

static void __pci_remove(struct pci_dev *pdev) {
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("\n");

	if (! self)
		return;

	remove_dev_attrs(&pdev->dev);
	dma_blocks_free(self);
	qvio_cdev_stop(&self->cdev);
	free_irqs(self);
	disable_msi_msix(self, pdev);
	unmap_bars(self);
	release_regions(self, pdev);
	qvio_device_put(self);
	dev_set_drvdata(&pdev->dev, NULL);
}

static pci_ers_result_t __pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, self);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, self);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t __pci_slot_reset(struct pci_dev *pdev)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("0x%p restart after slot reset\n", self);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", self);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void __pci_error_resume(struct pci_dev *pdev)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, self);
#if PCI_AER_NAMECHANGE
	pci_aer_clear_nonfatal_status(pdev);
#else
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif

}

#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
static void __pci_reset_prepare(struct pci_dev *pdev)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, self);
}

static void __pci_reset_done(struct pci_dev *pdev)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, self);
}

#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
static void __pci_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct qvio_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p, prepare %d.\n", pdev, self, prepare);
}
#endif

static const struct pci_error_handlers __pci_err_handler = {
	.error_detected	= __pci_error_detected,
	.slot_reset	= __pci_slot_reset,
	.resume		= __pci_error_resume,
#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
	.reset_prepare	= __pci_reset_prepare,
	.reset_done	= __pci_reset_done,
#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
	.reset_notify	= __pci_reset_notify,
#endif
};

static struct pci_driver pci_driver = {
	.name = DRV_MODULE_NAME,
	.id_table = __pci_ids,
	.probe = __pci_probe,
	.remove = __pci_remove,
	.err_handler = &__pci_err_handler,
};

int qvio_device_pci_register(void) {
	int err;

	pr_info("\n");

	err = pci_register_driver(&pci_driver);
	if(err) {
		pr_err("pci_register_driver() failed\n");
		goto err0;
	}

	return err;

err0:
	return err;
}

void qvio_device_pci_unregister(void) {
	pr_info("\n");

	pci_unregister_driver(&pci_driver);
}
