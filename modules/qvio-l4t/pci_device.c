#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/aer.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "pci_device.h"
// #include "device.h"
#include "xdma_desc.h"
#include "utils.h"

#include "xvidc.h"

#define DRV_MODULE_NAME "qvio-l4t"

#if defined(RHEL_RELEASE_CODE)
#	define PCI_AER_NAMECHANGE (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3))
#else
#	define PCI_AER_NAMECHANGE (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
#endif

#if 0
/* Interrupt Status and Control */
#define IE_AP_DONE		BIT(0)
#define IE_AP_READY		BIT(1)

#define ISR_AP_DONE_IRQ		BIT(0)
#define ISR_AP_READY_IRQ	BIT(1)

#define ISR_ALL_IRQ_MASK	\
		(ISR_AP_DONE_IRQ | \
		ISR_AP_READY_IRQ)
#endif

static ssize_t dma_block_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dma_block_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t dma_sync_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dma_sync_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t dma_block_index_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dma_block_index_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t test_case_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t test_case_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t zdev_ver_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t zdev_ticks_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t zdev_value0_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t zdev_value0_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static int map_single_bar(struct qvio_pci_device *self, int idx);
static int map_bars(struct qvio_pci_device* self);
static void unmap_bars(struct qvio_pci_device *self);
static void pci_enable_capability(struct pci_dev *pdev, int cap);
static void pci_check_intr_pend(struct pci_dev *pdev);
static void pci_keep_intx_enabled(struct pci_dev *pdev);
static int request_regions(struct qvio_pci_device *self);
static void release_regions(struct qvio_pci_device *self, struct pci_dev* pdev);
static int set_dma_mask(struct pci_dev *pdev);
#ifndef arch_msi_check_device
static int arch_msi_check_device(struct pci_dev *dev, int nvec, int type);
#endif
static int msi_msix_capable(struct pci_dev *dev, int type);
static irqreturn_t __irq_handler(int irq, void *dev_id);
static int enable_msi_msix(struct qvio_pci_device* self, struct pci_dev* pdev);
static void disable_msi_msix(struct qvio_pci_device* self, struct pci_dev* pdev);
static void free_irqs(struct qvio_pci_device* self);
static int irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev);
static void dma_blocks_free(struct qvio_pci_device* self);
static int dma_blocks_alloc(struct qvio_pci_device* self);
static int create_dev_attrs(struct device* dev);
static void remove_dev_attrs(struct device* dev);
static int __pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void __pci_remove(struct pci_dev *pdev);
static pci_ers_result_t __pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state);
static pci_ers_result_t __pci_slot_reset(struct pci_dev *pdev);
static void __pci_error_resume(struct pci_dev *pdev);
#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
static void __pci_reset_prepare(struct pci_dev *pdev);
static void __pci_reset_done(struct pci_dev *pdev);
#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
static void __pci_reset_notify(struct pci_dev *pdev, bool prepare);
#endif
static void __device_free(struct kref *ref);
static int device_probe(struct qvio_pci_device* self);
static void device_remove(struct qvio_pci_device* self);
static int device_7024_probe(struct qvio_pci_device* self);
static void device_7024_remove(struct qvio_pci_device* self);
static int device_7024_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev);

static const struct pci_device_id __pci_ids[] = {
	{ PCI_DEVICE(0x12AB, 0xE380), },
	{ PCI_DEVICE(0x12AB, 0xE381), },
	{ PCI_DEVICE(0x12AB, 0xE382), },
	{ PCI_DEVICE(0x10EE, 0x7024), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, __pci_ids);

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

static ssize_t dma_block_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	ssize_t count = PAGE_SIZE;

	memcpy(buf, p, count);

	return count;
}

static ssize_t dma_block_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	count = min(count, PAGE_SIZE);

	memcpy(p, buf, count);

	return count;
}

static ssize_t dma_sync_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->dma_sync);
}

static ssize_t dma_sync_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	struct dma_block_t* pDmaBlock = &self->dma_blocks[self->dma_block_index];
	int sync_type;

	sscanf(buf, "%d", &sync_type);

	switch(sync_type) {
	case 0:
		self->dma_sync = sync_type;
		dma_sync_single_for_cpu(dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_TO_DEVICE);
		break;

	case 1:
		self->dma_sync = sync_type;
		dma_sync_single_for_cpu(dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_FROM_DEVICE);
		break;

	case 2:
		self->dma_sync = sync_type;
		dma_sync_single_for_device(dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_TO_DEVICE);
		break;

	case 3:
		self->dma_sync = sync_type;
		dma_sync_single_for_device(dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_FROM_DEVICE);
		break;

	default:
		pr_err("unexpected value, sync_type=%d\n", sync_type);
		break;
	}

	return count;
}

static ssize_t dma_block_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->dma_block_index);
}

static ssize_t dma_block_index_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	int dma_block_index;

	sscanf(buf, "%d", &dma_block_index);

	if(dma_block_index >= 0 && dma_block_index < ARRAY_SIZE(self->dma_blocks)) {
		self->dma_block_index = dma_block_index;
	}

	return count;
}

static void do_test_case(struct qvio_pci_device* self, int test_case) {
	switch(test_case) {
	case 0:
		pr_info("+test_case %d\n", test_case);
		// do_test_case_0(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 1:
		pr_info("+test_case %d\n", test_case);
		// do_test_case_1(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 2:
		pr_info("+test_case %d\n", test_case);
		// do_test_case_2(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 3:
		pr_info("+test_case %d\n", test_case);
		// do_test_case_3(self);
		pr_info("-test_case %d\n", test_case);
		break;

	case 4:
		pr_info("+test_case %d\n", test_case);
		// do_test_case_4(self);
		pr_info("-test_case %d\n", test_case);
		break;

	default:
		pr_warn("unexpected test_case %d\n", test_case);
		break;
	}
}

static ssize_t test_case_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->test_case);
}

static ssize_t test_case_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	int test_case;

	sscanf(buf, "%d", &test_case);
	do_test_case(self, test_case);
	self->test_case = test_case;

	return count;
}

static ssize_t zdev_ver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	struct qvio_zdev* zdev = self->zdev;

	return qvio_zdev_attr_ver_show(zdev, buf);
}

static ssize_t zdev_ticks_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	struct qvio_zdev* zdev = self->zdev;

	return qvio_zdev_attr_ticks_show(zdev, buf);
}

static ssize_t zdev_value0_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	struct qvio_zdev* zdev = self->zdev;

	return qvio_zdev_attr_value0_show(zdev, buf);
}

static ssize_t zdev_value0_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct qvio_pci_device* self = dev_get_drvdata(dev);
	struct qvio_zdev* zdev = self->zdev;

	return qvio_zdev_attr_value0_store(zdev, buf, count);
}

#if 0
static int start_buf_entry(struct qvio_device* self, struct qvio_buf_entry* buf_entry) {
	int ret = 0;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	u32* xdma_h2c_sgdma;
	u32* xdma_c2h_sgdma;
	uintptr_t qdma_wr;
	uintptr_t qdma_rd;
	struct qvio_buf_regs* buf_regs = buf_entry->buf_regs;
	struct qvio_buffer* buf = &buf_entry->buf;
	struct dma_block_t* pDmaBlock;
	struct xdma_desc* pSgdmaDesc;

	switch(self->work_mode) {
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

	case QVIO_WORK_MODE_QDMA_RD:
		pDmaBlock = &buf_entry->desc_blocks[0];
		pSgdmaDesc = pDmaBlock->cpu_addr;

		pr_info("%d x %d, %x:%x\n", self->format.width, self->format.height,
			(u32)pSgdmaDesc->src_addr_hi, (u32)pSgdmaDesc->src_addr_lo);

		qdma_rd = (uintptr_t)self->qdma_rd;
		pr_info("qdma_rd[0x00]=0x%x\n", io_read_reg(qdma_rd, 0x00));
		io_write_reg(qdma_rd, 0x10, cpu_to_le32(PCI_DMA_L(buf_entry->dsc_adr)));
		io_write_reg(qdma_rd, 0x14, cpu_to_le32(PCI_DMA_H(buf_entry->dsc_adr)));
		io_write_reg(qdma_rd, 0x18, buf_entry->dsc_adj);
		io_write_reg(qdma_rd, 0x1C, self->format.width * self->format.height);
		io_write_reg(qdma_rd, 0x00, 0x01); // ap_start

		pr_info("QDMA RD started...\n");
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
#endif

static DEVICE_ATTR(dma_block, 0644, dma_block_show, dma_block_store);
static DEVICE_ATTR(dma_sync, 0664, dma_sync_show, dma_sync_store);
static DEVICE_ATTR(dma_block_index, 0644, dma_block_index_show, dma_block_index_store);
static DEVICE_ATTR(test_case, 0644, test_case_show, test_case_store);
static DEVICE_ATTR(zdev_ver, 0444, zdev_ver_show, NULL);
static DEVICE_ATTR(zdev_ticks, 0444, zdev_ticks_show, NULL);
static DEVICE_ATTR(zdev_value0, 0644, zdev_value0_show, zdev_value0_store);

#if 0
static int buf_entry_from_sgt(struct qvio_device* self, struct sg_table* sgt, struct qvio_buffer* buf, ssize_t buf_size, struct qvio_buf_entry** ppBufEntry) {
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
	ssize_t sg_bytes;

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
	case QVIO_WORK_MODE_AXIMM_TEST3:
	case QVIO_WORK_MODE_XDMA_H2C:
	case QVIO_WORK_MODE_QDMA_RD:
		pDmaBlock = &buf_entry->desc_blocks[0];
		ret = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
		if(ret) {
			pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
			goto err1;
		}

		if(nents >= PAGE_SIZE / sizeof(struct xdma_desc)) {
			ret = EINVAL;
			pr_err("unexpected value, nents=%d\n", nents);
			goto err1;
		}

		pSgdmaDesc = pDmaBlock->cpu_addr;
		Nxt_adj = nents - 1;
		dst_addr = 0xA0000000;

		sg_bytes = 0;
		for (i = 0; i < nents && sg_bytes < buf_size; i++, sg = sg_next(sg), pSgdmaDesc++, Nxt_adj--) {
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

		dma_sync_single_for_device(self->dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_FROM_DEVICE);
		break;

	case QVIO_WORK_MODE_AXIMM_TEST2:
	case QVIO_WORK_MODE_XDMA_C2H:
	case QVIO_WORK_MODE_QDMA_WR:
		pDmaBlock = &buf_entry->desc_blocks[0];
		ret = qvio_dma_block_alloc(pDmaBlock, buf_entry->desc_pool, GFP_KERNEL | GFP_DMA);
		if(ret) {
			pr_err("qvio_dma_block_alloc() failed, ret=%d\n", ret);
			goto err1;
		}

		if(nents >= PAGE_SIZE / sizeof(struct xdma_desc)) {
			ret = EINVAL;
			pr_err("unexpected value, nents=%d\n", nents);
			goto err1;
		}

		pSgdmaDesc = pDmaBlock->cpu_addr;
		Nxt_adj = nents - 1;
		src_addr = 0xA0000000;

		sg_bytes = 0;
		for (i = 0; i < nents && sg_bytes < buf_size; i++, sg = sg_next(sg), pSgdmaDesc++, Nxt_adj--) {
			dst_addr = sg_dma_address(sg);
			nxt_addr = pDmaBlock->dma_handle + ((u8*)(pSgdmaDesc + 1) - (u8*)pDmaBlock->cpu_addr);

#if 0
			pr_warn("%d, src_addr 0x%llx, dst_addr 0x%llx, nxt_addr 0x%llx, Nxt_adj %d len %d\n",
				i, src_addr, dst_addr, nxt_addr, Nxt_adj, sg_dma_len(sg));
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

			sg_bytes += sg_dma_len(sg);
			src_addr += sg_dma_len(sg);
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

		dma_sync_single_for_device(self->dev, pDmaBlock->dma_handle, PAGE_SIZE, DMA_FROM_DEVICE);
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
#endif

static int map_single_bar(struct qvio_pci_device *self, int idx)
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

static int map_bars(struct qvio_pci_device* self) {
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

static void unmap_bars(struct qvio_pci_device *self)
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

static int request_regions(struct qvio_pci_device *self)
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

static void release_regions(struct qvio_pci_device *self, struct pci_dev* pdev) {
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
	struct qvio_pci_device* self = dev_id;

	pr_info("IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;

	return IRQ_HANDLED;
}

#if 0
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

static irqreturn_t __irq_handler_qdma_rd(int irq, void *dev_id) {
	struct qvio_device* self = dev_id;
	uintptr_t qdma_rd = (uintptr_t)self->qdma_rd;
	u32 value;
	struct qvio_buf_entry* done_entry;
	struct qvio_buf_entry* next_entry;

#if 0
	pr_info("QDMA-RD, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;
#endif

#if 1
	value = io_read_reg(qdma_rd, 0x0C); // ISR (ap_done)
	if(! (value & 0x01)) {
		pr_warn("unexpected, value=%u\n", value);
		return IRQ_NONE;
	}

	io_write_reg(qdma_rd, 0x0C, value & 0x01); // ap_done, TOW

	if(value & 0x01) switch(1) { case 1:
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
			io_write_reg(qdma_rd, 0x10, cpu_to_le32(PCI_DMA_L(next_entry->dsc_adr)));
			io_write_reg(qdma_rd, 0x14, cpu_to_le32(PCI_DMA_H(next_entry->dsc_adr)));
			io_write_reg(qdma_rd, 0x18, next_entry->dsc_adj);
			io_write_reg(qdma_rd, 0x00, 0x01); // ap_start
		} else {
			pr_warn("unexpected value, next_entry=%llX\n", (int64_t)next_entry);
		}
	}
#endif

	return IRQ_HANDLED;
}
#endif

static int enable_msi_msix(struct qvio_pci_device* self, struct pci_dev* pdev) {
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

static void disable_msi_msix(struct qvio_pci_device* self, struct pci_dev* pdev) {
	if (self->msix_enabled) {
		pci_disable_msix(pdev);
		self->msix_enabled = 0;
	} else if (self->msi_enabled) {
		pci_disable_msi(pdev);
		self->msi_enabled = 0;
	}
}

static void free_irqs(struct qvio_pci_device* self) {
	int i;

	for(i = 0;i < ARRAY_SIZE(self->irq_lines);i++) {
		if(self->irq_lines[i]) {
			free_irq(self->irq_lines[i], self);
		}
	}
}

static int irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int err;

	switch(self->device_id & 0xFFFF) {
	case 0x7024:
		err = device_7024_irq_setup(self, pdev);
		if(err) {
			pr_err("device_7024_irq_setup() failed, err=%d\n", err);
			goto err0;
		}
		break;
	}

	return 0;

err0:
	free_irqs(self);

	return err;
}

static void dma_blocks_free(struct qvio_pci_device* self) {
	int i;

	for(i = 0;i < ARRAY_SIZE(self->dma_blocks);i++) {
		if(! self->dma_blocks[i].dma_handle)
			continue;

		qvio_dma_block_free(&self->dma_blocks[i], self->desc_pool);
	}
}

static int dma_blocks_alloc(struct qvio_pci_device* self) {
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

	err = device_create_file(dev, &dev_attr_dma_sync);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err1;
	}

	err = device_create_file(dev, &dev_attr_dma_block_index);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err2;
	}

	err = device_create_file(dev, &dev_attr_test_case);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err3;
	}

	err = device_create_file(dev, &dev_attr_zdev_ver);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err4;
	}

	err = device_create_file(dev, &dev_attr_zdev_ticks);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err5;
	}

	err = device_create_file(dev, &dev_attr_zdev_value0);
	if (err) {
		pr_err("device_create_file() failed, err=%d\n", err);
		goto err6;
	}

	return 0;


err6:
	device_remove_file(dev, &dev_attr_zdev_ticks);
err5:
	device_remove_file(dev, &dev_attr_zdev_ver);
err4:
	device_remove_file(dev, &dev_attr_test_case);
err3:
	device_remove_file(dev, &dev_attr_dma_block_index);
err2:
	device_remove_file(dev, &dev_attr_dma_sync);
err1:
	device_remove_file(dev, &dev_attr_dma_block);
err0:
	return err;
}

static void remove_dev_attrs(struct device* dev) {
	device_remove_file(dev, &dev_attr_zdev_value0);
	device_remove_file(dev, &dev_attr_zdev_ticks);
	device_remove_file(dev, &dev_attr_zdev_ver);
	device_remove_file(dev, &dev_attr_test_case);
	device_remove_file(dev, &dev_attr_dma_block_index);
	device_remove_file(dev, &dev_attr_dma_sync);
	device_remove_file(dev, &dev_attr_dma_block);
}

#if 0
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
	reset_mask = 0x07; // [aximm_test2_1, aximm_test3, aximm_test2]
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
#endif

static int __pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
	int err = 0;
	struct qvio_pci_device* self;

	pr_info("%04X:%04X (%04X:%04X)\n", (int)pdev->vendor, (int)pdev->device,
		(int)pdev->subsystem_vendor, (int)pdev->subsystem_device);

	self = qvio_pci_device_new();
	if(! self) {
		pr_err("qvio_pci_device_new() failed\n");
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

	err = device_probe(self);
	if(err) {
		pr_err("device_probe() failed, err=%d\n", err);
		goto err4;
	}

	err = irq_setup(self, pdev);
	if(err) {
		pr_err("irq_setup() failed, err=%d\n", err);
		goto err5;
	}

	self->desc_pool = dma_pool_create("desc_pool", &pdev->dev, PAGE_SIZE, 32, 0);
	if(!self->desc_pool) {
		pr_err("dma_pool_create() failed\n");
		err = -ENOMEM;
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
	dma_pool_destroy(self->desc_pool);
err6:
	free_irqs(self);
err5:
	device_remove(self);
err4:
	disable_msi_msix(self, pdev);
err3:
	unmap_bars(self);
err2:
	release_regions(self, pdev);
err1:
	qvio_pci_device_put(self);
err0:
	return err;
}

static void __pci_remove(struct pci_dev *pdev) {
	struct qvio_pci_device* self = dev_get_drvdata(&pdev->dev);

	pr_info("\n");

	if (! self)
		return;

	remove_dev_attrs(&pdev->dev);
	dma_blocks_free(self);
	dma_pool_destroy(self->desc_pool);
	free_irqs(self);
	device_remove(self);
	disable_msi_msix(self, pdev);
	unmap_bars(self);
	release_regions(self, pdev);
	qvio_pci_device_put(self);
	dev_set_drvdata(&pdev->dev, NULL);
}

static pci_ers_result_t __pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct qvio_pci_device* self = dev_get_drvdata(&pdev->dev);

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
	struct qvio_pci_device* self = dev_get_drvdata(&pdev->dev);

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
	struct qvio_pci_device* self = dev_get_drvdata(&pdev->dev);

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

int qvio_pci_device_register(void) {
	int err;

	pr_info("\n");

	err = qvio_zdev_register();
	if(err) {
		pr_err("qvio_zdev_register() failed\n");
		goto err0;
	}

	err = pci_register_driver(&pci_driver); // will trigger pci_probe
	if(err) {
		pr_err("pci_register_driver() failed\n");
		goto err1;
	}

	return err;

err1:
	qvio_zdev_unregister();
err0:
	return err;
}

void qvio_pci_device_unregister(void) {
	pr_info("\n");

	pci_unregister_driver(&pci_driver);
	qvio_zdev_unregister();
}

struct qvio_pci_device* qvio_pci_device_new(void) {
	int err;
	struct qvio_pci_device* self = kzalloc(sizeof(struct qvio_pci_device), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	self->zdev = qvio_zdev_new();
	if(! self->zdev) {
		pr_err("qvio_zdev_new() failed\n");
		err = -ENOMEM;
		goto err1;
	}

	self->qdma_wr = qvio_qdma_wr_new();
	if(! self->qdma_wr) {
		pr_err("qvio_qdma_wr_new() failed\n");
		err = -ENOMEM;
		goto err1;
	}

	return self;

err1:
	kfree(self);
err0:
	return NULL;
}

struct qvio_pci_device* qvio_pci_device_get(struct qvio_pci_device* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __device_free(struct kref *ref)
{
	struct qvio_pci_device* self = container_of(ref, struct qvio_pci_device, ref);

	// pr_info("\n");

	qvio_qdma_wr_put(self->qdma_wr);
	qvio_zdev_put(self->zdev);

	kfree(self);
}

void qvio_pci_device_put(struct qvio_pci_device* self) {
	if (self)
		kref_put(&self->ref, __device_free);
}

static int device_probe(struct qvio_pci_device* self) {
	int err;

	switch(self->device_id & 0xFFFF) {
#if 0
	case 0xE382:
		err = probe_e382(self);
		if(err) {
			pr_err("probe_e382() failed, err=%d\n", err);
			goto err0;
		}
		break;
#endif

	case 0x7024:
		err = device_7024_probe(self);
		if(err) {
			pr_err("device_7024_probe() failed, err=%d\n", err);
			goto err0;
		}
		break;

	default:
		err = -EINVAL;
		pr_err("unexpected value, self->device_id=0x%04X\n", (int)self->device_id);
		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static void device_remove(struct qvio_pci_device* self) {
	int err;

	switch(self->device_id & 0xFFFF) {
#if 0
	case 0xE382:
		device_e382_remove(self);
		break;
#endif

	case 0x7024:
		device_7024_remove(self);
		break;

	default:
		err = -EINVAL;
		pr_err("unexpected value, self->device_id=0x%04X\n", (int)self->device_id);
		goto err0;
		break;
	}

	return;

err0:
	return;
}

static int device_7024_probe(struct qvio_pci_device* self) {
	int err;

	self->reg_intr = (void __iomem *)((u64)self->bar[0] + 0x01000);

	self->zdev->dev = self->dev;
	self->zdev->device_id = self->device_id;
	self->zdev->reg = (void __iomem *)((u64)self->bar[0] + 0x00000);
	err = qvio_zdev_probe(self->zdev);
	if(err) {
		pr_err("qvio_zdev_probe() failed, err=%d\n", err);
		goto err0;
	}

	self->qdma_wr->dev = self->dev;
	self->qdma_wr->device_id = self->device_id;
	self->qdma_wr->reg_zdev = (void __iomem *)((u64)self->bar[0] + 0x00000);
	self->qdma_wr->reg = (void __iomem *)((u64)self->bar[0] + 0x10000);
	err = qvio_qdma_wr_probe(self->qdma_wr);
	if(err) {
		pr_err("qvio_qdma_wr_probe() failed, err=%d\n", err);
		goto err1;
	}

	return 0;

err1:
	qvio_zdev_remove(self->zdev);
err0:
	return 0;
}

static void device_7024_remove(struct qvio_pci_device* self) {
	qvio_qdma_wr_remove(self->qdma_wr);
	qvio_zdev_remove(self->zdev);
}

static int device_7024_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int err;
	int i;
	int irq_count;
	u32 value;
	uintptr_t reg_intr;
	u32 vector;
	irqreturn_t (*irq_handler_map[])(int irq, void *dev_id) = {
		qdma_wr_irq_handler,
		__irq_handler,
	};
	const char* irq_handler_name[] = {
		"qdma_wr",
		"noop"
	};
	void* irq_handler_dev[] = {
		self->qdma_wr,
		self
	};

	if(self->msi_enabled) {
		irq_count = pci_msi_vec_count(pdev);
		if(irq_count < 2) {
			pr_err("irq_count=%d", irq_count);
			err = -EINVAL;
			goto err0;
		}

		// IRQ Block User Vector Number
		reg_intr = (uintptr_t)self->reg_intr;
		value = io_read_reg(reg_intr, 0x00);
		value = (value & ~(0xFF << 0)) | (0 << 0); // Map usr_irq_req[0] to MSI-X Vector 0
		value = (value & ~(0xFF << 8)) | (1 << 8); // Map usr_irq_req[1] to MSI-X Vector 1
		io_write_reg(reg_intr, 0x00, value);

		for(i = 0;i < 2;i++) {
			vector = pci_irq_vector(pdev, i);
			if(vector == -EINVAL) {
				err = -EINVAL;
				pr_err("pci_irq_vector() failed\n");
				goto err0;
			}

			err = request_irq(vector, irq_handler_map[i], 0, irq_handler_name[i], irq_handler_dev[i]);
			if(err) {
				pr_err("%d: request_irq(%u) failed, err=%d\n", i, vector, err);
				goto err0;
			}
			self->irq_lines[i] = vector;
		}
	}

	return 0;

err0:
	return err;
}
