#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device.h"
#include "device.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/aer.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "xvidc.h"

#define DRV_MODULE_NAME "qvio-l4t"

/* Interrupt Status and Control */
#define IE_AP_DONE		BIT(0)
#define IE_AP_READY		BIT(1)

#define ISR_AP_DONE_IRQ		BIT(0)
#define ISR_AP_READY_IRQ		BIT(1)

#define ISR_ALL_IRQ_MASK	\
		(ISR_AP_DONE_IRQ | \
		ISR_AP_READY_IRQ)

enum {
	FOURCC_Y800 = 0x30303859,
	FOURCC_NV12 = 0x3231564E,
	FOURCC_NV16 = 0x3631564E,
};

inline uint32_t fourcc(char a, char b, char c, char d) {
	return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

inline uint32_t xdma_mkaddr(uint8_t target, uint8_t channel, uint8_t offset) {
	return ((uint32_t)(target & 0xF) << 12) | ((uint32_t)(channel & 0xF) << 8) | offset;
}

static const struct pci_device_id __pci_ids[] = {
	{ PCI_DEVICE(0x12AB, 0xE380), },
	{ PCI_DEVICE(0x12AB, 0xE381), },
	{ PCI_DEVICE(0x12AB, 0xE382), },
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

	for (i = 0; i < sgt->nents; i++, sg = sg_next(sg)) {
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

static ssize_t calc_buf_size(struct qvio_format* format, __u32 offset[4], __u32 stride[4]) {
	ssize_t ret;

	switch(format->fmt) {
	case FOURCC_Y800:
		ret = (ssize_t)(offset[0] + stride[0] * format->height);
		break;

	case FOURCC_NV16: // notice! offset[1] covers plane-0
		ret = (ssize_t)(offset[1] + stride[1] * format->height);
		break;

	case FOURCC_NV12: // notice! offset[1] covers plane-0
		ret = (ssize_t)(offset[1] + stride[1] * (format->height >> 1));
		break;

	default:
		pr_err("unexpected value, format->fmt=%08X\n", format->fmt);
		ret = -EINVAL;
		goto err0;
		break;
	}

	return ret;

err0:
	return ret;
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
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
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

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, PAGE_SIZE, DMA_TO_DEVICE);

	nIsIdle = XAximm_test1_IsIdle(xaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(xaximm_test1);
	XAximm_test1_InterruptEnable(xaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(xaximm_test1);

	XAximm_test1_Set_pDescItem(xaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(xaximm_test1, 1);
	XAximm_test1_Set_pDstPxl(xaximm_test1, self->dma_blocks[1].dma_handle);
	XAximm_test1_Set_nDstPxlStride(xaximm_test1, nStride);
	XAximm_test1_Set_nWidth(xaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(xaximm_test1, nHeight);

	XAximm_test1_Start(xaximm_test1);

	return;

err0:
	return;
}

static void do_test_case_1(struct qvio_device* self) {
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	u32 nIsIdle;

	XAximm_test1_InterruptClear(xaximm_test1, 0x01); // ap_done;

	nIsIdle = XAximm_test1_IsIdle(xaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
}

static void do_test_case_2(struct qvio_device* self) {
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
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

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, PAGE_SIZE, DMA_TO_DEVICE);

	nIsIdle = XAximm_test1_IsIdle(xaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(xaximm_test1);
	XAximm_test1_InterruptEnable(xaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(xaximm_test1);

	XAximm_test1_Set_pDescItem(xaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(xaximm_test1, nDescItems);
	XAximm_test1_Set_pDstPxl(xaximm_test1, pDstBase);
	XAximm_test1_Set_nDstPxlStride(xaximm_test1, nStride);
	XAximm_test1_Set_nWidth(xaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(xaximm_test1, nHeight);

	XAximm_test1_Start(xaximm_test1);
	return;

err0:
	return;
}

static void do_test_case_3(struct qvio_device* self) {
	int err;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_h2c_sgdma;
	dma_addr_t sgdma_desc;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	u8* pSrc;
	dma_addr_t dst_addr;
	dma_addr_t nxt_addr;
	int i;

	xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
	xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));
	xdma_h2c_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x4, 0, 0));
	sgdma_desc = self->dma_blocks[0].dma_handle;
	pSgdmaDesc = (struct xdma_desc*)self->dma_blocks[0].cpu_addr;
	src_addr = self->dma_blocks[1].dma_handle;
	pSrc = self->dma_blocks[1].cpu_addr;
	dst_addr = 0xA0000000;
	nxt_addr = 0;

	for(i = 0;i < 4096;i++) {
		pSrc[i] = i & 0xFF;
	}

	dma_sync_single_for_device(self->dev, src_addr, 4096, DMA_TO_DEVICE);

	pSgdmaDesc->control = cpu_to_le32(DESC_MAGIC | XDMA_DESC_STOPPED);
	pSgdmaDesc->bytes = cpu_to_le32(4096);
	pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
	pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
	pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
	pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
	pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
	pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));

	pr_info("H2C Channel Identifier: 0x%08X\n", xdma_h2c_channel[0x00 >> 2]);
	pr_info("H2C SGDMA Identifier: 0x%08X\n", xdma_h2c_sgdma[0x00 >> 2]);
	pr_info("H2C Channel Status: 0x%X\n", xdma_h2c_channel[0x40 >> 2]);
	pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

	xdma_irq_block[0x14 >> 2] = 0x01; // W1S engine_int_req[0:0]

	xdma_h2c_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(sgdma_desc));
	xdma_h2c_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(sgdma_desc));
	xdma_h2c_sgdma[0x88 >> 2] = 0;

	xdma_h2c_channel[0x04 >> 2] = BIT(0) | BIT(2); // Run & ie_descriptor_completed

	msleep(1000);

	pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

	xdma_h2c_channel[0x04 >> 2] = 0;
}

static void do_test_case_4(struct qvio_device* self) {
	int err;
	u32* xdma_irq_block;
	u32* xdma_c2h_channel;
	u32* xdma_c2h_sgdma;
	dma_addr_t sgdma_desc;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	u8* pDst;
	dma_addr_t nxt_addr;
	int i;

	xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
	xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));
	xdma_c2h_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x5, 0, 0));
	sgdma_desc = self->dma_blocks[0].dma_handle;
	pSgdmaDesc = (struct xdma_desc*)self->dma_blocks[0].cpu_addr;
	src_addr = 0xA0000000;
	dst_addr = self->dma_blocks[1].dma_handle;
	pDst = self->dma_blocks[1].cpu_addr;
	nxt_addr = 0;

	for(i = 0;i < 4096;i++) {
		pDst[i] = 0;
	}

	pSgdmaDesc->control = cpu_to_le32(DESC_MAGIC | XDMA_DESC_STOPPED | XDMA_DESC_COMPLETED);
	pSgdmaDesc->bytes = cpu_to_le32(4096);
	pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
	pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
	pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
	pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
	pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
	pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));

	pr_info("C2H Channel Identifier: 0x%08X\n", xdma_c2h_channel[0x00 >> 2]);
	pr_info("C2H SGDMA Identifier: 0x%08X\n", xdma_c2h_sgdma[0x00 >> 2]);
	pr_info("C2H Channel Status: 0x%X\n", xdma_c2h_channel[0x40 >> 2]);
	pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

	xdma_irq_block[0x14 >> 2] = 0x02; // W1S engine_int_req[1:1]

	xdma_c2h_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(sgdma_desc));
	xdma_c2h_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(sgdma_desc));
	xdma_c2h_sgdma[0x88 >> 2] = 0;

	xdma_c2h_channel[0x04 >> 2] = BIT(0) | BIT(2); // Run & ie_descriptor_completed

	msleep(1000);

	pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

	xdma_c2h_channel[0x04 >> 2] = 0;

	dma_sync_single_for_cpu(self->dev, dst_addr, 4096, DMA_FROM_DEVICE);
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

	case 3:
		pr_info("+test_case %d\n", test_case);
		do_test_case_3(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 4:
		pr_info("+test_case %d\n", test_case);
		do_test_case_4(self);
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

static int start_buf_entry(struct qvio_device* self, struct qvio_buf_entry* buf_entry) {
	int ret = 0;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	XV_tpg* xTpg = &self->xTpg;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	u32* xdma_h2c_sgdma;
	u32* xdma_c2h_sgdma;
	struct qvio_buf_regs* buf_regs = buf_entry->buf_regs;
	struct qvio_buffer* buf = &buf_entry->buf;

	switch(self->work_mode) {
	case QVIO_WORK_MODE_AXIMM_TEST0:
		XAximm_test0_DisableAutoRestart(xaximm_test0);
		XAximm_test0_InterruptEnable(xaximm_test0, 0x1); // ap_done
		XAximm_test0_InterruptGlobalEnable(xaximm_test0);
		XAximm_test0_Set_pDstPxl(xaximm_test0, self->dma_blocks[0].dma_handle);
		XAximm_test0_Set_nSize(xaximm_test0, self->format.width);
		XAximm_test0_Set_nTimes(xaximm_test0, self->format.height);
		XAximm_test0_Start(xaximm_test0);
		pr_info("XAximm_test0_Start()...\n");
		break;

	case QVIO_WORK_MODE_AXIMM_TEST1:
		XAximm_test1_DisableAutoRestart(xaximm_test1);
		XAximm_test1_InterruptEnable(xaximm_test1, 0x1); // ap_done
		XAximm_test1_InterruptGlobalEnable(xaximm_test1);
		XAximm_test1_Set_pDescItem(xaximm_test1, buf_regs[0].desc_dma_handle);
		XAximm_test1_Set_nDescItemCount(xaximm_test1, buf_regs[0].desc_items);
		XAximm_test1_Set_pDstPxl(xaximm_test1, buf_regs[0].dst_dma_handle);
		XAximm_test1_Set_nDstPxlStride(xaximm_test1, buf->stride[0]);
		XAximm_test1_Set_nWidth(xaximm_test1, self->format.width);
		XAximm_test1_Set_nHeight(xaximm_test1, self->format.height);
		XAximm_test1_Start(xaximm_test1);
		pr_info("XAximm_test1_Start()...\n");
		break;

	case QVIO_WORK_MODE_FRMBUFWR:
		XZ_frmbuf_writer_DisableAutoRestart(xFrmBufWr);
		XZ_frmbuf_writer_InterruptEnable(xFrmBufWr, 0x1); // ap_done
		XZ_frmbuf_writer_InterruptGlobalEnable(xFrmBufWr);
		XZ_frmbuf_writer_Set_pDescLuma(xFrmBufWr, buf_regs[0].desc_dma_handle);
		XZ_frmbuf_writer_Set_nDescLumaCount(xFrmBufWr, buf_regs[0].desc_items);
		XZ_frmbuf_writer_Set_pDstLuma(xFrmBufWr, buf_regs[0].dst_dma_handle);
		XZ_frmbuf_writer_Set_nDstLumaStride(xFrmBufWr, buf->stride[0]);
		XZ_frmbuf_writer_Set_pDescChroma(xFrmBufWr, buf_regs[1].desc_dma_handle);
		XZ_frmbuf_writer_Set_nDescChromaCount(xFrmBufWr, buf_regs[1].desc_items);
		XZ_frmbuf_writer_Set_pDstChroma(xFrmBufWr, buf_regs[1].dst_dma_handle);
		XZ_frmbuf_writer_Set_nDstChromaStride(xFrmBufWr, buf->stride[1]);
		XZ_frmbuf_writer_Set_nWidth(xFrmBufWr, self->format.width);
		XZ_frmbuf_writer_Set_nHeight(xFrmBufWr, self->format.height);
		XZ_frmbuf_writer_Start(xFrmBufWr);
		pr_info("XZ_frmbuf_writer_Start()...\n");

#if 1
		XV_tpg_EnableAutoRestart(xTpg);
		XV_tpg_InterruptGlobalDisable(xTpg);
#else
		XV_tpg_DisableAutoRestart(xTpg);
		XV_tpg_InterruptEnable(xTpg, 0x1); // ap_done
		XV_tpg_InterruptGlobalEnable(xTpg);
#endif
		XV_tpg_Set_colorFormat(xTpg, XVIDC_CSF_YCRCB_444);
		XV_tpg_Set_bckgndId(xTpg, XTPG_BKGND_V_RAMP);
		XV_tpg_Set_ovrlayId(xTpg, 0);
		XV_tpg_Set_enableInput(xTpg, 0);
		XV_tpg_Set_width(xTpg, self->format.width);
		XV_tpg_Set_height(xTpg, self->format.height);
		XV_tpg_Start(xTpg);
		pr_info("XV_tpg_Start()...\n");
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2:
		XAximm_test0_DisableAutoRestart(xaximm_test2);
		XAximm_test0_InterruptEnable(xaximm_test2, 0x1); // ap_done
		XAximm_test0_InterruptGlobalEnable(xaximm_test2);
		XAximm_test0_Set_pDstPxl(xaximm_test2, self->dma_blocks[0].dma_handle);
		XAximm_test0_Set_nSize(xaximm_test2, self->format.width);
		XAximm_test0_Set_nTimes(xaximm_test2, self->format.height);
		XAximm_test0_Start(xaximm_test2);
		pr_info("XAximm_test0_Start(xaximm_test2)...\n");
		break;

	case QVIO_WORK_MODE_AXIMM_TEST3:
		XAximm_test0_DisableAutoRestart(xaximm_test3);
		XAximm_test0_InterruptEnable(xaximm_test3, 0x1); // ap_done
		XAximm_test0_InterruptGlobalEnable(xaximm_test3);
		XAximm_test0_Set_pDstPxl(xaximm_test3, self->dma_blocks[0].dma_handle);
		XAximm_test0_Set_nSize(xaximm_test3, self->format.width);
		XAximm_test0_Set_nTimes(xaximm_test3, self->format.height);
		XAximm_test0_Start(xaximm_test3);
		pr_info("XAximm_test0_Start(xaximm_test3)...\n");
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2_1:
		XAximm_test0_DisableAutoRestart(xaximm_test2_1);
		XAximm_test0_InterruptEnable(xaximm_test2_1, 0x1); // ap_done
		XAximm_test0_InterruptGlobalEnable(xaximm_test2_1);
		XAximm_test0_Set_pDstPxl(xaximm_test2_1, self->dma_blocks[0].dma_handle);
		XAximm_test0_Set_nSize(xaximm_test2_1, self->format.width);
		XAximm_test0_Set_nTimes(xaximm_test2_1, self->format.height);
		XAximm_test0_Start(xaximm_test2_1);
		pr_info("XAximm_test0_Start(xaximm_test2_1)...\n");
		break;

	case QVIO_WORK_MODE_XDMA_H2C:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));
		xdma_h2c_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x4, 0, 0));

#if 1
		xdma_irq_block[0x14 >> 2] = 0x01; // W1S engine_int_req[0:0]
		xdma_h2c_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr));
		xdma_h2c_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr));
		xdma_h2c_sgdma[0x88 >> 2] = buf_entry->dsc_adj;
		xdma_h2c_channel[0x04 >> 2] = BIT(0) | BIT(1); // Run & ie_descriptor_stopped
#endif
		pr_info("XDMA H2C started...\n");
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));
		xdma_c2h_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x5, 0, 0));

#if 1
		xdma_irq_block[0x14 >> 2] = 0x02; // W1S engine_int_req[1:1]
		xdma_c2h_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr));
		xdma_c2h_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr));
		xdma_c2h_sgdma[0x88 >> 2] = buf_entry->dsc_adj;
		xdma_c2h_channel[0x04 >> 2] = BIT(0) | BIT(1); // Run & ie_descriptor_stopped
#endif
		pr_info("XDMA C2H started...\n");
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = -EINVAL;
		goto err0;
		break;
	}

	return 0;

err0:
	return ret;
}

static DEVICE_ATTR(dma_block, 0644, dma_block_show, dma_block_store);
static DEVICE_ATTR(dma_block_index, 0644, dma_block_index_show, dma_block_index_store);
static DEVICE_ATTR(test_case, 0644, test_case_show, test_case_store);
static DEVICE_ATTR(zzlab_ver, 0444, zzlab_ver_show, NULL);
static DEVICE_ATTR(zzlab_ticks, 0444, zzlab_ticks_show, NULL);

static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static long __file_ioctl_g_ticks(struct file * filp, unsigned long arg);
static long __file_ioctl_s_fmt(struct file * filp, unsigned long arg);
static long __file_ioctl_g_fmt(struct file * filp, unsigned long arg);
static long __file_ioctl_req_bufs(struct file * filp, unsigned long arg);
static long __file_ioctl_query_buf(struct file * filp, unsigned long arg);
static long __file_ioctl_qbuf(struct file * filp, unsigned long arg);
static long __file_ioctl_dqbuf(struct file * filp, unsigned long arg);
static long __file_ioctl_streamon(struct file * filp, unsigned long arg);
static long __file_ioctl_streamoff(struct file * filp, unsigned long arg);
static long __file_ioctl_s_work_mode(struct file * filp, unsigned long arg);
static long __file_ioctl_g_work_mode(struct file * filp, unsigned long arg);

static __poll_t __file_poll(struct file *filp, struct poll_table_struct *wait) {
	struct qvio_device* self = filp->private_data;

	poll_wait(filp, &self->irq_wait, wait);

	if (! list_empty(&self->done_list))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
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

	case QVIO_IOC_REQ_BUFS:
		ret = __file_ioctl_req_bufs(filp, arg);
		break;

	case QVIO_IOC_QUERY_BUF:
		ret = __file_ioctl_query_buf(filp, arg);
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

	case QVIO_IOC_S_WORK_MODE:
		ret = __file_ioctl_s_work_mode(filp, arg);
		break;

	case QVIO_IOC_G_WORK_MODE:
		ret = __file_ioctl_g_work_mode(filp, arg);
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

	return 0;

err0:
	return ret;
}

static long __file_ioctl_s_fmt(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_format args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	switch(args.fmt) {
	case FOURCC_Y800:
		self->planes = 1;
		break;

	case FOURCC_NV12:
	case FOURCC_NV16:
		self->planes = 2;
		break;

	default:
		self->planes = 0;

		ret = -EINVAL;
		goto err0;
		break;
	}

	self->format = args;
	pr_info("%dx%d 0x%08X\n", self->format.width, self->format.height, self->format.fmt);

	return 0;

err0:
	return ret;
}

static long __file_ioctl_g_fmt(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_format args;

	args = self->format;

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

static long __file_ioctl_req_bufs(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_req_bufs args;
	ssize_t buf_size;
	size_t mmap_buffer_size;
	int i;
	__u32 offset;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	if(self->buffers) {
		kfree(self->buffers);
		self->buffers_count = 0;
	}

	self->buffers = kcalloc(args.count, sizeof(struct qvio_buffer), GFP_KERNEL);
	if(! self->buffers) {
		pr_err("kcalloc() failed\n");
		goto err0;
	}
	self->buffers_count = args.count;

	for(i = 0;i < args.count;i++) {
		self->buffers[i].index = i;
		self->buffers[i].buf_type = args.buf_type;
		memcpy(self->buffers[i].offset, args.offset, ARRAY_SIZE(args.offset));
		memcpy(self->buffers[i].stride, args.stride, ARRAY_SIZE(args.stride));
	}

	buf_size = calc_buf_size(&self->format, args.offset, args.stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}
	pr_info("buf_size=%lu\n", buf_size);

	switch(args.buf_type) {
	case QVIO_BUF_TYPE_MMAP:
		if(self->mmap_buffer) vfree(self->mmap_buffer);

		mmap_buffer_size = (size_t)(buf_size * args.count);
		self->mmap_buffer = vmalloc(mmap_buffer_size);
		if(! self->mmap_buffer) {
			pr_err("vmalloc() failed\n");
			goto err0;
		}

		offset = 0;
		for(i = 0;i < args.count;i++) {
			self->buffers[i].u.offset = offset;

			offset += buf_size;
		}

		break;

	case QVIO_BUF_TYPE_DMABUF:
		break;

	case QVIO_BUF_TYPE_USERPTR:
		break;

	default:
		pr_err("unexpected value, args.buf_type=%d\n", (int)args.buf_type);
		ret = -EINVAL;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_query_buf(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	struct qvio_buffer args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	if(args.index >= self->buffers_count) {
		pr_err("unexpected value, %u >= %u\n", args.index, self->buffers_count);

		ret = -EINVAL;
		goto err0;
	}

	args = self->buffers[args.index];

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

static void skip_offset_sgt(int offset, struct scatterlist** pSg, int nents, int* pSgIndex, int* pSgOffset) {
	struct scatterlist* sg = *pSg;
	int sg_index = *pSgIndex;
	int sg_offset = *pSgOffset;

#if 0
	pr_info("%d/%d: sg_offset=%d offset=%d\n", sg_index, nents, sg_offset, offset);
#endif

	while(sg_index < nents) {
		offset -= (int)sg_dma_len(sg) - sg_offset;

#if 0
		pr_info("%d: offset=%d\n", sg_index, offset);
#endif

		if(offset == 0) {
			sg = sg_next(sg);
			sg_index++;
			sg_offset = 0;
			break;
		} else if(offset < 0) {
			sg_offset = (int)sg_dma_len(sg) + offset;
			break;
		}

		sg = sg_next(sg);
		sg_index++;
		sg_offset = 0;
	}

	*pSg = sg;
	*pSgIndex = sg_index;
	*pSgOffset = sg_offset;

#if 0
	pr_info("%d: sg_offset=%d\n", sg_index, sg_offset);
#endif
}

static int get_desc_items_sgt(int buf_size, struct scatterlist** pSg, int nents, int* pSgIndex, int* pSgOffset, struct desc_item_t* pSgDescItems, struct qvio_buf_regs* buf_regs) {
	struct scatterlist* sg;
	int sg_index = *pSgIndex;
	int sg_offset;
	dma_addr_t pDstBase;
	struct desc_item_t* pDescItem;
	int64_t nDstOffset;

	if(sg_index >= nents) {
		pr_err("unexpected, %d >= %d", sg_index, nents);
		return -EPERM;
	}

	sg = *pSg;
	sg_offset = *pSgOffset;
	pDescItem = pSgDescItems;
	pDstBase = sg_dma_address(sg);

#if 0
	pr_info("%d/%d: sg_offset=%d\n", sg_index, nents, sg_offset);
#endif

#if 1
	while(sg_index < nents) {
		nDstOffset = sg_dma_address(sg) + sg_offset - pDstBase;
		pDescItem->nOffsetHigh = ((nDstOffset >> 32) & 0xFFFFFFFF);
		pDescItem->nOffsetLow = (nDstOffset & 0xFFFFFFFF);
		pDescItem->nSize = (size_t)sg_dma_len(sg) - sg_offset;
		buf_size -= (int)sg_dma_len(sg) - sg_offset;

#if 0
		pr_info("%d: sg_offset=%d (0x%08X 0x%08X) 0x%08X, buf_size=%d\n", sg_index, sg_offset,
			pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, buf_size);
#endif

		if(buf_size == 0) {
			pDescItem++;
			sg = sg_next(sg);
			sg_index++;
			sg_offset = 0;
			break;
		} else if(buf_size < 0) {
			pDescItem->nSize += buf_size;
			pDescItem++;
			sg_offset = (int)sg_dma_len(sg) + buf_size;
			break;
		}

		pDescItem++;
		sg = sg_next(sg);
		sg_index++;
		sg_offset = 0;
	}
#endif

	*pSg = sg;
	*pSgIndex = sg_index;
	*pSgOffset = sg_offset;
	buf_regs->dst_dma_handle = pDstBase;

#if 0
	pr_info("%d: sg_offset=%d\n", sg_index, sg_offset);
#endif

	return (int)(pDescItem - pSgDescItems);
}

static int fill_desc_items_block(struct desc_item_t* pDescItem, int nDescItemCount,
	struct dma_block_t** ppDescBlock, struct dma_block_t* pDescBlockEnd, int nDescBlockSize,
	struct qvio_buf_entry* buf_entry, struct qvio_buf_regs* buf_regs) {
	int ret;
	struct dma_block_t* pDescBlock = *ppDescBlock;
	int nMaxDescItemsInBlock = (nDescBlockSize >> DESC_BITSHIFT) - 1; // reserve one desc item for next/end link
	int nDescItemsInBlock;
	dma_addr_t pDescBase;
	struct desc_item_t* pDescItemInBlock;
	int64_t nDescOffset;

	// prepare 1st desc block
	ret = qvio_dma_block_alloc(pDescBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
	if(ret) {
		pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
		goto err0;
	}
	nDescItemsInBlock = min(nDescItemCount, nMaxDescItemsInBlock);

	pDescBase = pDescBlock->dma_handle;
	buf_regs->desc_dma_handle = pDescBase;
	buf_regs->desc_items = nDescItemsInBlock;

	while(true) {
		pDescItemInBlock = pDescBlock->cpu_addr;

		memcpy(pDescItemInBlock, pDescItem, sizeof(struct desc_item_t) * nDescItemsInBlock);
		pDescItem += nDescItemsInBlock;
		nDescItemCount -= nDescItemsInBlock;
		pDescItemInBlock += nDescItemsInBlock;

		if(nDescItemCount <= 0) {
			// end link
			pDescItemInBlock->nOffsetHigh = 0;
			pDescItemInBlock->nOffsetLow = 0;
			pDescItemInBlock->nSize = 0;
			pDescItemInBlock->nFlags = 0;

			// sync current desc block to device
			dma_sync_single_for_device(buf_entry->dev, pDescBlock->dma_handle, nDescBlockSize, DMA_TO_DEVICE);
			break;
		}

		// alloc next desc block
		if(++pDescBlock == pDescBlockEnd) {
			pr_err("out of desc blocks\n");
			ret = -ENOMEM;
			goto err0;
		}
		ret = qvio_dma_block_alloc(pDescBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
		if(ret) {
			pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
			goto err0;
		}
		nDescItemsInBlock = min(nDescItemCount, nMaxDescItemsInBlock);

		// next link
		nDescOffset = pDescBlock->dma_handle - pDescBase;
		pDescItemInBlock->nOffsetHigh = ((nDescOffset >> 32) & 0xFFFFFFFF);
		pDescItemInBlock->nOffsetLow = (nDescOffset & 0xFFFFFFFF);
		pDescItemInBlock->nSize = nDescItemsInBlock;
		pDescItemInBlock->nFlags = 0;

		pr_warn("next link, (%llx, %d)", (int64_t)nDescOffset, nDescItemsInBlock);

		// sync last desc block to device
		dma_sync_single_for_device(buf_entry->dev, pDescBlock[-1].dma_handle, nDescBlockSize, DMA_TO_DEVICE);
	}

	*ppDescBlock = pDescBlock;

	return 0;

err0:
	return ret;
}

static int buf_entry_from_sgt(struct qvio_device* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry** ppBufEntry) {
	int ret;
	struct qvio_buf_entry* buf_entry;
	struct desc_item_t* pSgDescItems;
	struct scatterlist* sg;
	int nents;
	int sg_index;
	int sg_offset;
	struct dma_block_t* pDescBlock;
	struct dma_block_t* pDescBlockEnd;
	struct qvio_buf_regs* buf_regs;
	int nDescItemCount;
	int i;
	struct dma_block_t* pDmaBlock;
	struct xdma_desc* pSgdmaDesc;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	dma_addr_t nxt_addr;
	int Nxt_adj;

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = ENOMEM;
		goto err0;
	}

	buf_entry->dev = self->dev;
	buf_entry->desc_pool = self->desc_pool;

	pSgDescItems = NULL;
	sg = sgt->sgl;
	nents = sgt->nents;

	switch(self->work_mode) {
	case QVIO_WORK_MODE_AXIMM_TEST0:
	case QVIO_WORK_MODE_AXIMM_TEST1:
	case QVIO_WORK_MODE_FRMBUFWR:
	case QVIO_WORK_MODE_AXIMM_TEST2:
	case QVIO_WORK_MODE_AXIMM_TEST3:
	case QVIO_WORK_MODE_AXIMM_TEST2_1:
		sg_index = 0;
		sg_offset = 0;
		pDescBlock = buf_entry->desc_blocks;
		pDescBlockEnd = buf_entry->desc_blocks + ARRAY_SIZE(buf_entry->desc_blocks);
		buf_regs = buf_entry->buf_regs;

		pSgDescItems = vmalloc(sizeof(struct desc_item_t) * nents);
		if(! pSgDescItems) {
			pr_err("vmalloc() failed\n");
			ret = ENOMEM;
			goto err1;
		}

		for(i = 0;i < self->planes;i++) {
			// pr_info("plane %d\n", i);

			skip_offset_sgt(
				buf->offset[i],
				&sg, nents, &sg_index, &sg_offset);
			nDescItemCount = get_desc_items_sgt(
				buf->stride[i] * self->format.height,
				&sg, nents, &sg_index, &sg_offset,
				pSgDescItems, &buf_regs[i]);

			// pr_info("nDescItemCount=%d, sg_index=%d, sg_offset=%d\n", nDescItemCount, sg_index, sg_offset);

			ret = fill_desc_items_block(
				pSgDescItems, nDescItemCount,
				&pDescBlock, pDescBlockEnd, self->desc_block_size,
				buf_entry, &buf_regs[i]);
			if(ret) {
				pr_err("fill_desc_items_block() failed, ret=%d\n", ret);
				goto err2;
			}

			sg = sgt->sgl;
			sg_index = 0;
			sg_offset = 0;
		}

		vfree(pSgDescItems);
		break;

	case QVIO_WORK_MODE_XDMA_H2C:
		pDmaBlock = &buf_entry->desc_blocks[0];
		ret = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
		if(ret) {
			pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
			goto err1;
		}

		buf_entry->dsc_adr = pDmaBlock->dma_handle;
		buf_entry->dsc_adj = nents - 1;

		pSgdmaDesc = pDmaBlock->cpu_addr;
		Nxt_adj = nents - 1;
		dst_addr = 0xA0000000;

		for (i = 0; i < nents; i++, sg = sg_next(sg), pSgdmaDesc++, dst_addr += sg_dma_len(sg), Nxt_adj--) {
			src_addr = sg_dma_address(sg);
			nxt_addr = pDmaBlock->dma_handle + ((u8*)(pSgdmaDesc + 1) - (u8*)pDmaBlock->cpu_addr);

#if 0
			pr_warn("%d, src_addr 0x%llx, dst_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d\n",
				i, src_addr, dst_addr, nxt_addr, Nxt_adj);
#endif

#if 1
			pSgdmaDesc->control = cpu_to_le32(DESC_MAGIC | (Nxt_adj << 8));
			pSgdmaDesc->bytes = sg_dma_len(sg);
			pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
			pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
			pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
			pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
			pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
			pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));
#endif
		}

#if 1
		pSgdmaDesc--;
		pSgdmaDesc->control |= cpu_to_le32(XDMA_DESC_STOPPED);
		pSgdmaDesc->next_lo = 0;
		pSgdmaDesc->next_hi = 0;
#endif
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		pDmaBlock = &buf_entry->desc_blocks[0];
		ret = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
		if(ret) {
			pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
			goto err1;
		}

		buf_entry->dsc_adr = pDmaBlock->dma_handle;
		buf_entry->dsc_adj = nents - 1;

		pSgdmaDesc = pDmaBlock->cpu_addr;
		Nxt_adj = nents - 1;
		src_addr = 0xA0000000;

		for (i = 0; i < nents; i++, sg = sg_next(sg), pSgdmaDesc++, dst_addr += sg_dma_len(sg), Nxt_adj--) {
			dst_addr = sg_dma_address(sg);
			nxt_addr = pDmaBlock->dma_handle + ((u8*)(pSgdmaDesc + 1) - (u8*)pDmaBlock->cpu_addr);

#if 0
			pr_warn("%d, src_addr 0x%llx, dst_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d\n",
				i, src_addr, dst_addr, nxt_addr, Nxt_adj);
#endif

#if 1
			pSgdmaDesc->control = cpu_to_le32(DESC_MAGIC | (Nxt_adj << 8));
			pSgdmaDesc->bytes = sg_dma_len(sg);
			pSgdmaDesc->src_addr_lo = cpu_to_le32(PCI_DMA_L(src_addr));
			pSgdmaDesc->src_addr_hi = cpu_to_le32(PCI_DMA_H(src_addr));
			pSgdmaDesc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(dst_addr));
			pSgdmaDesc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(dst_addr));
			pSgdmaDesc->next_lo = cpu_to_le32(PCI_DMA_L(nxt_addr));
			pSgdmaDesc->next_hi = cpu_to_le32(PCI_DMA_H(nxt_addr));
#endif
		}

#if 1
		pSgdmaDesc--;
		pSgdmaDesc->control |= cpu_to_le32(XDMA_DESC_STOPPED);
		pSgdmaDesc->next_lo = 0;
		pSgdmaDesc->next_hi = 0;
#endif
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = EINVAL;
		goto err1;
	}

	*ppBufEntry = buf_entry;

	return 0;

err2:
	if(pSgDescItems) vfree(pSgDescItems);
err1:
	qvio_buf_entry_put(buf_entry);
err0:
	return ret;
}

static long __file_ioctl_qbuf_userptr(struct file * filp, struct qvio_buffer* buf) {
	struct qvio_device* self = filp->private_data;
	long ret;
	ssize_t buf_size;
	unsigned long start;
	unsigned long length;
	unsigned long first, last;
	unsigned long nr;
	struct frame_vector *vec;
	unsigned int flags_vec = FOLL_FORCE | FOLL_WRITE;
	int nr_pages;
	struct sg_table* sgt;
	enum dma_data_direction dma_dir = DMA_FROM_DEVICE;
	struct qvio_buf_entry* buf_entry;
	struct qvio_buf_entry* next_entry;
	unsigned long flags;

	start = buf->u.userptr;

	buf_size = calc_buf_size(&self->format, buf->offset, buf->stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}
	// pr_info("buf_size=%lu\n", buf_size);

	length = buf_size;
	first = start >> PAGE_SHIFT;
	last = (start + length - 1) >> PAGE_SHIFT;
	nr = last - first + 1;
	// pr_info("start=%lX, length=%lu, nr=%lu\n", start, length, nr);

	vec = frame_vector_create(nr);
	if(! vec) {
		pr_err("frame_vector_create() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

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

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (! sgt) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	ret = sg_alloc_table_from_pages(sgt, frame_vector_pages(vec), nr_pages, 0, length, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed, err=%ld\n", ret);
		goto err3;
	}

	ret = dma_map_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (ret) {
		pr_err("dma_map_sgtable() failed, err=%ld\n", ret);
		goto err4;
	}

	ret = buf_entry_from_sgt(self, sgt, buf, &buf_entry);
	if (ret) {
		pr_err("buf_entry_from_sgt() failed, err=%ld\n", ret);
		goto err5;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.userptr.sgt = sgt;

	spin_lock_irqsave(&self->lock, flags);
	next_entry = list_empty(&self->job_list) ? buf_entry : NULL;
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->lock, flags);

#if 1
	if(self->state == QVIO_STATE_START && next_entry) {
		start_buf_entry(self, next_entry);
	}
#endif

	return 0;

err5:
	dma_unmap_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
err4:
	sg_free_table(sgt);
err3:
	kfree(sgt);
err2:
	put_vaddr_frames(vec);
err1:
	frame_vector_destroy(vec);
err0:
	return ret;
}

static long __file_ioctl_qbuf_dmabuf(struct file * filp, struct qvio_buffer* buf) {
	struct qvio_device* self = filp->private_data;
	long ret;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	enum dma_data_direction dma_dir = DMA_FROM_DEVICE;
	struct qvio_buf_entry* buf_entry;
	struct qvio_buf_entry* next_entry;
	unsigned long flags;

	dmabuf = dma_buf_get(buf->u.fd);
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

	ret = dma_buf_begin_cpu_access(dmabuf, dma_dir);
	if (ret) {
		pr_err("dma_buf_begin_cpu_access() failed, ret=%ld\n", ret);
		goto err2;
	}

	sgt = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR(sgt)) {
		pr_err("dma_buf_map_attachment() failed, sgt=%p\n", sgt);

		ret = -EINVAL;
		goto err3;
	}

	// sgt_dump(sgt, true);

	ret = buf_entry_from_sgt(self, sgt, buf, &buf_entry);
	if (ret) {
		pr_err("buf_entry_from_sgt() failed, err=%ld\n", ret);
		goto err4;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.dmabuf.dmabuf = dmabuf;
	buf_entry->u.dmabuf.attach = attach;
	buf_entry->u.dmabuf.sgt = sgt;

	spin_lock_irqsave(&self->lock, flags);
	next_entry = list_empty(&self->job_list) ? buf_entry : NULL;
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->lock, flags);

#if 1
	if(self->state == QVIO_STATE_START && next_entry) {
		start_buf_entry(self, next_entry);
	}
#endif

	return 0;

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

static long __file_ioctl_qbuf_mmap(struct file * filp, struct qvio_buffer* buf) {
	struct qvio_device* self = filp->private_data;
	long ret;
	ssize_t buf_size;
	void* vaddr;
	unsigned long nr_pages;
	struct page **pages;
	unsigned long i;
	struct sg_table* sgt;
	enum dma_data_direction dma_dir = DMA_FROM_DEVICE;
	struct qvio_buf_entry* buf_entry;
	struct qvio_buf_entry* next_entry;
	unsigned long flags;

	buf_size = calc_buf_size(&self->format, buf->offset, buf->stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}
	// pr_info("buf_size=%lu\n", buf_size);

	vaddr = (uint8_t*)self->mmap_buffer + buf->u.offset;
	// pr_info("vaddr=%llX\n", (int64_t)vaddr);

	nr_pages = (buf_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	// pr_info("nr_pages=%lu\n", nr_pages);

	pages = kmalloc(sizeof(struct page *) * nr_pages, GFP_KERNEL);
	if (!pages) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	for (i = 0; i < nr_pages; i++) {
		pages[i] = vmalloc_to_page(vaddr + (i << PAGE_SHIFT));
		if (!pages[i]) {
			pr_err("vmalloc_to_page() failed\n");
			ret = -ENOMEM;
			goto err1;
		}
	}

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err1;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, nr_pages, 0, buf_size, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed, err=%ld\n", ret);
		goto err2;
	}

	ret = dma_map_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (ret) {
		pr_err("dma_map_sgtable() failed, err=%ld\n", ret);
		goto err3;
	}

	// sgt_dump(sgt, true);

	ret = buf_entry_from_sgt(self, sgt, buf, &buf_entry);
	if (ret) {
		pr_err("buf_entry_from_sgt() failed, err=%ld\n", ret);
		goto err4;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.userptr.sgt = sgt;

	spin_lock_irqsave(&self->lock, flags);
	next_entry = list_empty(&self->job_list) ? buf_entry : NULL;
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->lock, flags);

#if 1
	if(self->state == QVIO_STATE_START && next_entry) {
		start_buf_entry(self, next_entry);
	}
#endif

	kfree(pages);

	return 0;

err4:
	dma_unmap_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
err3:
	sg_free_table(sgt);
err2:
	kfree(sgt);
err1:
	kfree(pages);
err0:
	return ret;
}

static long __file_ioctl_qbuf(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_buffer buf;

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	switch(buf.buf_type) {
	case QVIO_BUF_TYPE_USERPTR:
		ret = __file_ioctl_qbuf_userptr(filp, &buf);
		break;

	case QVIO_BUF_TYPE_DMABUF:
		ret = __file_ioctl_qbuf_dmabuf(filp, &buf);
		break;

	case QVIO_BUF_TYPE_MMAP:
		ret = __file_ioctl_qbuf_mmap(filp, &buf);
		break;

	default:
		pr_err("unexpected value, buf.buf_type=%d\n", buf.buf_type);
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
	struct qvio_buffer args;

	spin_lock_irqsave(&self->lock, flags);
	if(list_empty(&self->done_list)) {
		spin_unlock_irqrestore(&self->lock, flags);

		ret = -EAGAIN;
		goto err0;
	}

	buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
	list_del(&buf_entry->node);
	spin_unlock_irqrestore(&self->lock, flags);

	// pr_info("buf_entry->buf.index=0x%llX\n", (int64_t)buf_entry->buf.index);
	args.index = buf_entry->buf.index;

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
	struct pci_dev* pdev = self->pci_dev;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	XV_tpg* xTpg = &self->xTpg;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	long ret;
	struct qvio_buf_entry* buf_entry;
	u32 value;
	u32 nIsIdle;
	int i;
	int reset_mask;

	if(self->state == QVIO_STATE_START) {
		pr_err("unexpected value, self->state=%d\n", self->state);
		ret = -EINVAL;
		goto err0;
	}

	switch(self->work_mode) {
	case QVIO_WORK_MODE_AXIMM_TEST0:
		switch(pdev->device) {
		case 0xE380:
			reset_mask = 0x10; // [aximm_test0, z_frmbuf_writer, aximm_test1, pcie_intr, tpg]
			break;

		case 0xE381:
			reset_mask = 0x04; // [aximm_test3, aximm_test2, pcie_intr]
			break;

		default:
			reset_mask = 0;
			pr_err("unexpected value, pdev->device=0x%04X\n", (int)pdev->device);
			break;
		}

		pr_info("reset aximm_test0...\n");
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~reset_mask;
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | reset_mask;

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test0);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_AXIMM_TEST1:
		pr_info("reset aximm_test1...\n"); // [x, x, aximm_test1, x, x]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x04; // 00100
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x04; // 00100

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test1_IsIdle(xaximm_test1);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_FRMBUFWR:
		pr_info("reset z_frmbuf_writer, tpg...\n"); // [x, z_frmbuf_writer, x, x, tpg]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x09; // 01001
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x09; // 01001

		for(i = 0;i < 5;i++) {
			nIsIdle = XZ_frmbuf_writer_IsIdle(xFrmBufWr);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		for(i = 0;i < 5;i++) {
			nIsIdle = XV_tpg_IsIdle(xTpg);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XV_tpg_IsIdle()=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2:
		pr_info("reset aximm_test2...\n");
		reset_mask = 0x02; // [aximm_test3, aximm_test2, pcie_intr]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x2; // 10
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x2; // 10

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test2);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_AXIMM_TEST3:
		pr_info("reset aximm_test3...\n");
		reset_mask = 0x04; // [aximm_test3, aximm_test2, pcie_intr]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~reset_mask;
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | reset_mask;

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test3);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2_1:
		pr_info("reset aximm_test2_1...\n");
		reset_mask = 0x02; // [aximm_test3, aximm_test2, pcie_intr]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x2; // 10
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x2; // 10

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test2_1);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_XDMA_H2C:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("H2C Channel Identifier: 0x%08X\n", xdma_h2c_channel[0x00 >> 2]);
		pr_info("H2C Channel Status: 0x%X\n", xdma_h2c_channel[0x40 >> 2]);
		pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x14 >> 2] = 0x01; // W1S engine_int_req[0:0]
		xdma_h2c_channel[0x04 >> 2] = 0;
#endif
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("C2H Channel Identifier: 0x%08X\n", xdma_c2h_channel[0x00 >> 2]);
		pr_info("C2H Channel Status: 0x%X\n", xdma_c2h_channel[0x40 >> 2]);
		pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x14 >> 2] = 0x02; // W1S engine_int_req[1:1]
		xdma_c2h_channel[0x04 >> 2] = 0;
#endif
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = -EINVAL;
		goto err0;
		break;
	}

	if(! list_empty(&self->job_list)) {
		buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
		ret = start_buf_entry(self, buf_entry);
		if(ret) {
			pr_err("start_buf_entry() failed, err=%d\n", (int)ret);
			goto err0;
		}
	}

	self->state = QVIO_STATE_START;

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamoff(struct file * filp, unsigned long arg) {
	struct qvio_device* self = filp->private_data;
	long ret;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	XV_tpg* xTpg = &self->xTpg;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	u32 value;
	u32 nIsIdle;
	int i;
	unsigned long flags;
	struct qvio_buf_entry* buf_entry;

	if(self->state == QVIO_STATE_READY) {
		pr_err("unexpected value, self->state=%d\n", self->state);
		ret = -EINVAL;
		goto err0;
	}

	switch(self->work_mode) {
	case QVIO_WORK_MODE_AXIMM_TEST0:
		XAximm_test0_InterruptGlobalDisable(xaximm_test0);

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test0);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		XAximm_test0_InterruptClear(xaximm_test0, ISR_ALL_IRQ_MASK);
		break;

	case QVIO_WORK_MODE_AXIMM_TEST1:
		XAximm_test1_InterruptGlobalDisable(xaximm_test1);

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test1_IsIdle(xaximm_test1);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		XAximm_test1_InterruptClear(xaximm_test1, ISR_ALL_IRQ_MASK);
		break;

	case QVIO_WORK_MODE_FRMBUFWR:
		XZ_frmbuf_writer_InterruptGlobalDisable(xFrmBufWr);

		pr_info("reset tpg...\n"); // [x, x, x, x, tpg]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x01; // 00001
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x01; // 00001

		for(i = 0;i < 5;i++) {
			nIsIdle = XV_tpg_IsIdle(xTpg);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("unexpected, XV_tpg_IsIdle()=%u\n", nIsIdle);
		}

		pr_info("reset z_frmbuf_writer...\n"); // [x, z_frmbuf_writer, x, x, x]
		value = *(u32*)((u8*)self->zzlab_env + 0x24);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~0x08; // 01000
		msleep(100);
		*(u32*)((u8*)self->zzlab_env + 0x24) = value | 0x08; // 01000

		for(i = 0;i < 5;i++) {
			nIsIdle = XZ_frmbuf_writer_IsIdle(xFrmBufWr);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2:
		XAximm_test0_InterruptGlobalDisable(xaximm_test2);

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test2);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		XAximm_test0_InterruptClear(xaximm_test2, ISR_ALL_IRQ_MASK);
		break;

	case QVIO_WORK_MODE_AXIMM_TEST3:
		XAximm_test0_InterruptGlobalDisable(xaximm_test3);

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test3);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		XAximm_test0_InterruptClear(xaximm_test3, ISR_ALL_IRQ_MASK);
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2_1:
		XAximm_test0_InterruptGlobalDisable(xaximm_test2_1);

		for(i = 0;i < 5;i++) {
			nIsIdle = XAximm_test0_IsIdle(xaximm_test2_1);
			if(nIsIdle)
				break;
			msleep(100);
		}
		if(! nIsIdle) {
			pr_err("nIsIdle=%u\n", nIsIdle);
		}

		XAximm_test0_InterruptClear(xaximm_test2_1, ISR_ALL_IRQ_MASK);
		break;

	case QVIO_WORK_MODE_XDMA_H2C:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("H2C Channel Identifier: 0x%08X\n", xdma_h2c_channel[0x00 >> 2]);
		pr_info("H2C Channel Status: 0x%X\n", xdma_h2c_channel[0x40 >> 2]);
		pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x18 >> 2] = 0x01; // W1C engine_int_req[0:0]
		xdma_h2c_channel[0x04 >> 2] = 0;
#endif
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("C2H Channel Identifier: 0x%08X\n", xdma_c2h_channel[0x00 >> 2]);
		pr_info("C2H Channel Status: 0x%X\n", xdma_c2h_channel[0x40 >> 2]);
		pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x18 >> 2] = 0x02; // W1C engine_int_req[1:1]
		xdma_c2h_channel[0x04 >> 2] = 0;
#endif
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = -EINVAL;
		goto err0;
		break;
	}

	spin_lock_irqsave(&self->lock, flags);
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
	spin_unlock_irqrestore(&self->lock, flags);

	self->state = QVIO_STATE_READY;

	return 0;

err0:
	return ret;
}

static long __file_ioctl_s_work_mode(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	int args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	self->work_mode = args;

	pr_info("work_mode=%d\n", self->work_mode);

	return 0;

err0:
	return ret;
}

static long __file_ioctl_g_work_mode(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_device* self = filp->private_data;
	int args;

	args = self->work_mode;

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

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
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

static irqreturn_t __irq_handler_xaximm_test1(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;
	struct qvio_buf_regs* buf_regs;
	struct qvio_buffer* buf;

#if 0
	pr_info("AXIMM_TEST1, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test1_InterruptGetStatus(xaximm_test1);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XAximm_test1_InterruptClear(xaximm_test1, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			buf_regs = next_entry->buf_regs;
			buf = &next_entry->buf;

			XAximm_test1_Set_pDescItem(xaximm_test1, buf_regs[0].desc_dma_handle);
			XAximm_test1_Set_nDescItemCount(xaximm_test1, buf_regs[0].desc_items);
			XAximm_test1_Set_pDstPxl(xaximm_test1, buf_regs[0].dst_dma_handle);
			XAximm_test1_Set_nDstPxlStride(xaximm_test1, buf->stride[0]);
			XAximm_test1_Start(xaximm_test1);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_tpg(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;

	pr_info("TPG, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_frmbufwr(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;
	struct qvio_buf_regs* buf_regs;
	struct qvio_buffer* buf;

#if 0
	pr_info("FrmBufWr, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XZ_frmbuf_writer_InterruptGetStatus(xFrmBufWr);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XZ_frmbuf_writer_InterruptClear(xFrmBufWr, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			buf_regs = next_entry->buf_regs;
			buf = &next_entry->buf;

			XZ_frmbuf_writer_Set_pDescLuma(xFrmBufWr, buf_regs[0].desc_dma_handle);
			XZ_frmbuf_writer_Set_nDescLumaCount(xFrmBufWr, buf_regs[0].desc_items);
			XZ_frmbuf_writer_Set_pDstLuma(xFrmBufWr, buf_regs[0].dst_dma_handle);
			XZ_frmbuf_writer_Set_nDstLumaStride(xFrmBufWr, buf->stride[0]);
			XZ_frmbuf_writer_Set_pDescChroma(xFrmBufWr, buf_regs[1].desc_dma_handle);
			XZ_frmbuf_writer_Set_nDescChromaCount(xFrmBufWr, buf_regs[1].desc_items);
			XZ_frmbuf_writer_Set_pDstChroma(xFrmBufWr, buf_regs[1].dst_dma_handle);
			XZ_frmbuf_writer_Set_nDstChromaStride(xFrmBufWr, buf->stride[1]);
			XZ_frmbuf_writer_Start(xFrmBufWr);
			// pr_warn("XZ_frmbuf_writer_Start\n");
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_xaximm_test0(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("AXIMM_TEST0, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test0_InterruptGetStatus(xaximm_test0);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XAximm_test0_InterruptClear(xaximm_test0, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			XAximm_test0_Start(xaximm_test0);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_xaximm_test2(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("AXIMM_TEST2, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test0_InterruptGetStatus(xaximm_test2);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XAximm_test0_InterruptClear(xaximm_test2, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			XAximm_test0_Start(xaximm_test2);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_xaximm_test3(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("AXIMM_TEST3, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test0_InterruptGetStatus(xaximm_test3);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XAximm_test0_InterruptClear(xaximm_test3, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			XAximm_test0_Start(xaximm_test3);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_xaximm_test2_1(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	u32 status;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("AXIMM_TEST2_1, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

	status = XAximm_test0_InterruptGetStatus(xaximm_test2_1);
	if(! (status & ISR_ALL_IRQ_MASK)) {
		pr_warn("unexpected, status=%u\n", status);
		return IRQ_NONE;
	}

	XAximm_test0_InterruptClear(xaximm_test2_1, status & ISR_ALL_IRQ_MASK);

	if(status & ISR_AP_DONE_IRQ) switch(1) { case 1:
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
			XAximm_test0_Start(xaximm_test2_1);
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t __irq_handler_xdma(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	u32* zzlab_env;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	u32* xdma_h2c_sgdma;
	u32* xdma_c2h_sgdma;
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

	if(engine_int_req & 0x02) switch(1) { case 1: // C2H
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));
		xdma_c2h_sgdma = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x5, 0, 0));

		xdma_irq_block[0x18 >> 2] = 0x02; // W1C engine_int_req[1:1]
		Status = xdma_c2h_channel[0x44 >> 2];
		pr_info("Engine Interrupt C2H, Status=%d\n", Status);

		xdma_c2h_channel[0x04 >> 2] = 0; // Stop

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
			xdma_irq_block[0x14 >> 2] = 0x02; // W1S engine_int_req[1:1]
			xdma_c2h_sgdma[0x80 >> 2] = cpu_to_le32(PCI_DMA_L(next_entry->dsc_adr));
			xdma_c2h_sgdma[0x84 >> 2] = cpu_to_le32(PCI_DMA_H(next_entry->dsc_adr));
			xdma_c2h_sgdma[0x88 >> 2] = next_entry->dsc_adj;
			xdma_c2h_channel[0x04 >> 2] = BIT(0) | BIT(1); // Run & ie_descriptor_stopped
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}

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
	int irq_count;

	if(self->msi_enabled) {
		irq_count = min((int)pci_msi_vec_count(pdev), (int)ARRAY_SIZE(self->irq_lines));

		for(i = 0;i < irq_count;i++) {
			vector = pci_irq_vector(pdev, i);
			if(vector == -EINVAL) {
				err = -EINVAL;
				pr_err("pci_irq_vector() failed\n");
				goto err0;
			}

			pr_info("irq[%d] vector=%d\n", i, vector);

			irq_handler = __irq_handler;

			switch(pdev->device) {
			case 0xE380:
				switch(i) {
				case 0:
					irq_handler = __irq_handler_xaximm_test1;
					break;

				case 1:
					irq_handler = __irq_handler_tpg;
					break;

				case 2:
					irq_handler = __irq_handler_frmbufwr;
					break;

				case 3:
					irq_handler = __irq_handler_xaximm_test0;
					break;
				}
				break;

			case 0xE381:
				switch(i) {
				case 0:
					irq_handler = __irq_handler_xaximm_test2;
					break;

				case 1:
					irq_handler = __irq_handler_xaximm_test3;
					break;

				case 2:
					irq_handler = __irq_handler_xaximm_test2_1;
					break;
				}
				break;

			case 0xE382:
				switch(i) {
				case 0:
					irq_handler = __irq_handler_xdma;
					break;

				case 1:
					irq_handler = __irq_handler_xdma;
					break;

				case 2:
					irq_handler = __irq_handler_xdma;
					break;

				case 3:
					irq_handler = __irq_handler_xdma;
					break;
				}
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

static int setup_e380(struct qvio_device* self) {
	int err;
	int i;
	u32 nIsIdle;
	u32 value;
	int reset_mask;

#if 1
	pr_info("reset all IPs...\n");
	reset_mask = 0x1F; // [aximm_test0, z_frmbuf_writer, aximm_test1, pcie_intr, tpg]
	value = *(u32*)((u8*)self->zzlab_env + 0x24);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~reset_mask;
	msleep(100);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value | reset_mask;
#endif

	self->xaximm_test0.Control_BaseAddress = (u64)self->bar[0] + 0x40000;
	self->xaximm_test0.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test0 = %p\n", (void*)self->xaximm_test0.Control_BaseAddress);

	self->xaximm_test1.Control_BaseAddress = (u64)self->bar[0] + 0x20000;
	self->xaximm_test1.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test1 = %p\n", (void*)self->xaximm_test1.Control_BaseAddress);

	self->xFrmBufWr.Control_BaseAddress = (u64)self->bar[0] + 0x30000;
	self->xFrmBufWr.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xFrmBufWr = %p\n", (void*)self->xFrmBufWr.Control_BaseAddress);

    self->xTpg.Config.DeviceId = 0;
    self->xTpg.Config.BaseAddress = (u64)self->bar[0] + 0x10000;
    self->xTpg.Config.HasAxi4sSlave = 0;
    self->xTpg.Config.PixPerClk = 8;
    self->xTpg.Config.NumVidComponents = 3;
    self->xTpg.Config.MaxWidth = 4096;
    self->xTpg.Config.MaxHeight = 2160;
    self->xTpg.Config.MaxDataWidth = 10;
	self->xTpg.Config.SolidColorEnable = 0;
	self->xTpg.Config.RampEnable = 1;
	self->xTpg.Config.ColorBarEnable = 0;
	self->xTpg.Config.DisplayPortEnable = 0;
	self->xTpg.Config.ColorSweepEnable = 0;
	self->xTpg.Config.ZoneplateEnable = 0;
	self->xTpg.Config.ForegroundEnable = 0;
    self->xTpg.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xTpg = %p\n", (void*)self->xTpg.Config.BaseAddress);

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test0_IsIdle(&self->xaximm_test0);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
	}

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test1_IsIdle(&self->xaximm_test1);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XAximm_test1_IsIdle()=%u\n", nIsIdle);
	}

	for(i = 0;i < 5;i++) {
		nIsIdle = XZ_frmbuf_writer_IsIdle(&self->xFrmBufWr);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("nIsIdle=%u\n", nIsIdle);
	}

	for(i = 0;i < 5;i++) {
		nIsIdle = XV_tpg_IsIdle(&self->xTpg);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XV_tpg_IsIdle()=%u\n", nIsIdle);
	}

	return 0;
}

static int setup_e381(struct qvio_device* self) {
	int err;
	struct pci_dev *pdev = self->pci_dev;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	int i;
	u32 nIsIdle;
	u32 value;
	int reset_mask;

#if 1
	pr_info("reset all IPs...\n");
	reset_mask = 0x07; // [aximm_test3, aximm_test2, pcie_intr]
	value = *(u32*)((u8*)self->zzlab_env + 0x24);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value & ~reset_mask;
	msleep(100);
	*(u32*)((u8*)self->zzlab_env + 0x24) = value | reset_mask;
#endif

	xaximm_test2->Control_BaseAddress = (u64)self->bar[0] + 0x10000;
	xaximm_test2->IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test2 = %p\n", (void*)xaximm_test2->Control_BaseAddress);

	xaximm_test3->Control_BaseAddress = (u64)self->bar[0] + 0x20000;
	xaximm_test3->IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test3 = %p\n", (void*)xaximm_test3->Control_BaseAddress);

	xaximm_test2_1->Control_BaseAddress = (u64)self->bar[0] + 0x30000;
	xaximm_test2_1->IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test2_1 = %p\n", (void*)xaximm_test2_1->Control_BaseAddress);

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test0_IsIdle(xaximm_test2);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
	}

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test0_IsIdle(xaximm_test3);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
	}

	for(i = 0;i < 5;i++) {
		nIsIdle = XAximm_test0_IsIdle(xaximm_test2_1);
		if(nIsIdle)
			break;
		msleep(100);
	}
	if(! nIsIdle) {
		pr_err("unexpected, XAximm_test0_IsIdle()=%u\n", nIsIdle);
	}

	return 0;
}

static int setup_e382(struct qvio_device* self) {
	int err;
	struct pci_dev *pdev = self->pci_dev;
	u32* xdma_irq_block;
	u32 xdma_reg;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;

	xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
	xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));
	xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));

	// IRQ Block User Interrupt Enable Mask
	xdma_irq_block[0x04 >> 2] = 0x0F; // Enable usr_irq_req[3:0]

	// IRQ Block User Vector Number
	xdma_reg = xdma_irq_block[0x80 >> 2];
	xdma_reg = (xdma_reg & ~(0x1F << 0)) | (0 << 0); // Map usr_irq_req[0] to MSI-X Vector 0
	xdma_reg = (xdma_reg & ~(0x1F << 8)) | (1 << 8); // Map usr_irq_req[1] to MSI-X Vector 1
	xdma_reg = (xdma_reg & ~(0x1F << 16)) | (2 << 16); // Map usr_irq_req[2] to MSI-X Vector 2
	xdma_reg = (xdma_reg & ~(0x1F << 24)) | (3 << 24); // Map usr_irq_req[3] to MSI-X Vector 3
	xdma_irq_block[0x80 >> 2] = xdma_reg;

	// IRQ Block Channel Interrupt Enable Mask
	xdma_irq_block[0x10 >> 2] = 0x03; // Enable engine_int_req[1:0]
	xdma_irq_block[0x18 >> 2] = 0x03; // W1C engine_int_req[1:0]

	// IRQ Block Channel Vector Number
	xdma_reg = xdma_irq_block[0xA0 >> 2];
	xdma_reg = (xdma_reg & ~(0x1F << 0)) | (0 << 0); // Map engine_int_req[0] to MSI-X Vector 0
	xdma_reg = (xdma_reg & ~(0x1F << 8)) | (1 << 8); // Map engine_int_req[1] to MSI-X Vector 1
	xdma_irq_block[0xA0 >> 2] = xdma_reg;

	xdma_h2c_channel[0x90 >> 2] = BIT(1); // im_descriptor_stopped
	xdma_c2h_channel[0x90 >> 2] = BIT(1); // im_descriptor_stopped

	return 0;
}

static int __pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
	int err = 0;
	struct qvio_device* self;

	pr_info("%04X:%04X (%04X:%04X)\n", (int)pdev->vendor, (int)pdev->device,
		(int)pdev->subsystem_vendor, (int)pdev->subsystem_device);

	self = qvio_device_new();
	if(! self) {
		pr_err("qvio_device_new() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	self->dev = &pdev->dev;
	self->pci_dev = pdev;
	dev_set_drvdata(&pdev->dev, self);
	self->device_id = (((int)pdev->vendor & 0xFFFF) << 16) |
		((int)pdev->device & 0xFFFF);
	self->device_subid = (((int)pdev->subsystem_vendor & 0xFFFF) << 16) |
		((int)pdev->subsystem_device & 0xFFFF);

	pr_info("device_id=%08X device_subid=%08X\n", self->device_id, self->device_subid);

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

	switch(pdev->device) {
	case 0xE380:
		err = setup_e380(self);
		if(err) {
			pr_err("setup_e380() failed, err=%d\n", err);
			goto err5;
		}
		break;

	case 0xE381:
		err = setup_e381(self);
		if(err) {
			pr_err("setup_e381() failed, err=%d\n", err);
			goto err5;
		}
		break;

	case 0xE382:
		err = setup_e382(self);
		if(err) {
			pr_err("setup_e382() failed, err=%d\n", err);
			goto err5;
		}
		break;

	default:
		err = -EINVAL;
		pr_err("unexpected value, pdev->device=0x%04X\n", (int)pdev->device);
		goto err5;
		break;
	}

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err5;
	}

	self->desc_block_size = PAGE_SIZE;
	self->desc_pool = dma_pool_create("desc_pool", &pdev->dev, self->desc_block_size, DESC_SIZE, 0);
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
