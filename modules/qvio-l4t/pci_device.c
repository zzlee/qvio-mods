#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device.h"
#include "device.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/aer.h>
#include <linux/pci.h>

#define DRV_MODULE_NAME "qvio-l4t"

static const struct pci_device_id __pci_ids[] = {
	{ PCI_DEVICE(0x12AB, 0xE380), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, __pci_ids);

ssize_t dma_block_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	ssize_t count = min(PAGE_SIZE, self->dma_blocks[self->dma_block_index].size);

	memcpy(buf, p, count);

	return count;
}

ssize_t dma_block_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	__u8* p = (__u8*)self->dma_blocks[self->dma_block_index].cpu_addr;
	count = min(count, self->dma_blocks[self->dma_block_index].size);

	memcpy(p, buf, count);

	return count;
}

ssize_t dma_block_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->dma_block_index);
}

ssize_t dma_block_index_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	int dma_block_index;

	sscanf(buf, "%d", &dma_block_index);

	if(dma_block_index >= 0 && dma_block_index < ARRAY_SIZE(self->dma_blocks)) {
		self->dma_block_index = dma_block_index;
	}

	return count;
}

#define DESC_BITSHIFT				4
#define DESC_SIZE					(1 << DESC_BITSHIFT)

struct __attribute__((__packed__)) desc_item_t {
	s32 nOffsetHigh;
	u32 nOffsetLow;
	u32 nSize;
	u32 nFlags;
};

void do_test_case_0(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	int nDescItems = 1;
	int nWidth = 64;
	int nHeight = 32;
	int nStride = nWidth + 16;
	struct desc_item_t* pDescItem;
	u64 nDescOffset;
	u64 nOffset;
	u32 nIsIdle;

	pDescItem = self->dma_blocks[0].cpu_addr;

	// desc-item header
	nDescOffset = (u8*)(&pDescItem[1]) - (u8*)self->dma_blocks[0].cpu_addr;
	pDescItem->nOffsetHigh = ((nDescOffset >> 32) & 0xFFFFFFFF);
	pDescItem->nOffsetLow = (nDescOffset & 0xFFFFFFFF);
	pDescItem->nSize = nDescItems << DESC_BITSHIFT;
	pDescItem->nFlags = 0;
	pr_info("(%08X %08X) %08X %08X\n", pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
	pDescItem++;

	// buffer desc-item
	nOffset = 0;
	pDescItem->nOffsetHigh = ((nOffset >> 32) & 0xFFFFFFFF);
	pDescItem->nOffsetLow = (nOffset & 0xFFFFFFFF);
	pDescItem->nSize = nStride * nHeight;
	pDescItem->nFlags = 0;
	pr_info("(%08X %08X) %08X %08X\n", pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
	pDescItem++;

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, self->dma_blocks[0].size, DMA_BIDIRECTIONAL);

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(pXaximm_test1);
	XAximm_test1_InterruptEnable(pXaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(pXaximm_test1);

	XAximm_test1_Set_pDescItem(pXaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(pXaximm_test1, 2);
	XAximm_test1_Set_pDstPxl(pXaximm_test1, self->dma_blocks[1].dma_handle);
	XAximm_test1_Set_nDstPxlStride(pXaximm_test1, nStride);
	XAximm_test1_Set_nWidth(pXaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(pXaximm_test1, nHeight);

	XAximm_test1_Start(pXaximm_test1);

	return;

err0:
	return;
}

void do_test_case_1(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	u32 nIsIdle;

	XAximm_test1_InterruptClear(pXaximm_test1, 0x01); // ap_done;

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
}

void do_test_case_2(struct qvio_device* self) {
	XAximm_test1* pXaximm_test1 = &self->xaximm_test1;
	int nDescItems = 3;
	int nWidth = 64;
	int nHeight = 128;
	int nStride = nWidth + 16;
	struct desc_item_t* pDescBase;
	struct desc_item_t* pDescItem;
	dma_addr_t pDstBase;
	size_t nDstSize = nStride * nHeight;
	u64 nDescOffset;
	u64 nDstOffset;
	int nDmaBlockIndex = 1;
	u32 nIsIdle;

	pDescBase = self->dma_blocks[0].cpu_addr;
	pDstBase = self->dma_blocks[1].dma_handle;
	pDescItem = self->dma_blocks[0].cpu_addr;

	// desc-item header
	nDescOffset = (u8*)(pDescItem + 1) - (u8*)pDescBase;
	pDescItem->nOffsetHigh = ((nDescOffset >> 32) & 0xFFFFFFFF);
	pDescItem->nOffsetLow = (nDescOffset & 0xFFFFFFFF);
	pDescItem->nSize = nDescItems << DESC_BITSHIFT;
	pDescItem->nFlags = 0;
	pr_info("[*] (%08X %08X) %08X %08X\n", pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
	pDescItem++;

	// buffer desc-item
	do {
		nDstOffset = self->dma_blocks[nDmaBlockIndex].dma_handle - pDstBase;
		pDescItem->nOffsetHigh = ((nDstOffset >> 32) & 0xFFFFFFFF);
		pDescItem->nOffsetLow = (nDstOffset & 0xFFFFFFFF);
		pDescItem->nSize = min(self->dma_blocks[nDmaBlockIndex].size, nDstSize);
		pDescItem->nFlags = 0;
		pr_info("[%d] (%08X %08X) %08X %08X\n", nDmaBlockIndex, pDescItem->nOffsetHigh, pDescItem->nOffsetLow, pDescItem->nSize, pDescItem->nFlags);
		pDescItem++;

		nDstSize -= pDescItem->nSize;
		nDmaBlockIndex++;
	} while(nDstSize > 0);
	pr_warn("nDmaBlockIndex=%u\n", nDmaBlockIndex);

	dma_sync_single_for_device(self->dev, self->dma_blocks[0].dma_handle, self->dma_blocks[0].size, DMA_BIDIRECTIONAL);

	nIsIdle = XAximm_test1_IsIdle(pXaximm_test1);
	pr_info("XAximm_test1_IsIdle()=%u\n", nIsIdle);
	if(! nIsIdle) {
		goto err0;
	}

	XAximm_test1_DisableAutoRestart(pXaximm_test1);
	XAximm_test1_InterruptEnable(pXaximm_test1, 0x1); // ap_done
	XAximm_test1_InterruptGlobalEnable(pXaximm_test1);

	XAximm_test1_Set_pDescItem(pXaximm_test1, self->dma_blocks[0].dma_handle);
	XAximm_test1_Set_nDescItemCount(pXaximm_test1, 2);
	XAximm_test1_Set_pDstPxl(pXaximm_test1, pDstBase);
	XAximm_test1_Set_nDstPxlStride(pXaximm_test1, nStride);
	XAximm_test1_Set_nWidth(pXaximm_test1, nWidth);
	XAximm_test1_Set_nHeight(pXaximm_test1, nHeight);

	XAximm_test1_Start(pXaximm_test1);
	return;

err0:
	return;
}

void do_test_case(struct qvio_device* self, int test_case) {
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

ssize_t test_case_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qvio_device* self = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", self->test_case);
}

ssize_t test_case_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct qvio_device* self = dev_get_drvdata(dev);
	int test_case;

	sscanf(buf, "%d", &test_case);
	do_test_case(self, test_case);
	self->test_case = test_case;

	return count;
}

static DEVICE_ATTR(dma_block, 0644, dma_block_show, dma_block_store);
static DEVICE_ATTR(dma_block_index, 0644, dma_block_index_show, dma_block_index_store);
static DEVICE_ATTR(test_case, 0644, test_case_show, test_case_store);

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_device* device = filp->private_data;

	pr_info("device=%p, cmd=%u\n", device, cmd);

	switch(cmd) {
	default:
		pr_err("unexpected, cmd=%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
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

	pr_info("AXIMM_TEST1, IRQ[%d]: irq_counter=%d\n", irq, self->irq_counter);
	self->irq_counter++;

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

static void dma_blocks_free(struct qvio_device* self, struct pci_dev* pdev) {
	int i;

	for(i = 0;i < ARRAY_SIZE(self->dma_blocks);i++) {
		if(! self->dma_blocks[i].dma_handle)
			continue;

		dma_free_coherent(&pdev->dev, self->dma_blocks[i].size, self->dma_blocks[i].cpu_addr, self->dma_blocks[i].dma_handle);
	}
}

static int dma_blocks_alloc(struct qvio_device* self, struct pci_dev* pdev) {
	int err;
	int i;

	for(i = 0;i < ARRAY_SIZE(self->dma_blocks);i++) {
		self->dma_blocks[i].size = PAGE_SIZE;
		self->dma_blocks[i].gfp = GFP_KERNEL | GFP_DMA;
		self->dma_blocks[i].cpu_addr = dma_alloc_coherent(&pdev->dev, self->dma_blocks[i].size,
			&self->dma_blocks[i].dma_handle, self->dma_blocks[i].gfp);
		if(err) {
			pr_err("dma_alloc_coherent() failed, err=%d\n", err);
			goto err0;
		}

		pr_info("dma_blocks[%d]: size=%d cpu_addr=%llx dma_handle=%llx", i,
			(int)self->dma_blocks[i].size, (u64)self->dma_blocks[i].cpu_addr, (u64)self->dma_blocks[i].dma_handle);
	}

	return 0;

err0:
	dma_blocks_free(self, pdev);

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

	return 0;

err2:
	device_remove_file(dev, &dev_attr_test_case);
err1:
	device_remove_file(dev, &dev_attr_dma_block);
err0:
	return err;
}

static void remove_dev_attrs(struct device* dev) {
	device_remove_file(dev, &dev_attr_test_case);
	device_remove_file(dev, &dev_attr_dma_block_index);
	device_remove_file(dev, &dev_attr_dma_block);
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

	self->zzlab_env = (void __iomem *)(self->bar[0] + 0x10000);
	pr_info("zzlab_env = %p\n", self->zzlab_env);

	self->xaximm_test1.Control_BaseAddress = (u64)self->bar[0] + 0x20000;
	self->xaximm_test1.IsReady = XIL_COMPONENT_IS_READY;
	pr_info("xaximm_test1 = %p\n", (void*)self->xaximm_test1.Control_BaseAddress);

	pr_info("Version: %08X %08X %08X\n",
		*(u32*)((u8*)self->zzlab_env + 0x10),
		*(u32*)((u8*)self->zzlab_env + 0x14),
		*(u32*)((u8*)self->zzlab_env + 0x18));

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err5;
	}

	err = dma_blocks_alloc(self, pdev);
	if(err) {
		pr_err("dma_blocks_alloc() failed, err=%d\n", err);
		goto err6;
	}

	err = create_dev_attrs(&pdev->dev);
	if (err) {
		pr_err("create_dev_attrs() failed, err=%d\n", err);
		goto err7;
	}

	return 0;

err7:
	dma_blocks_free(self, pdev);
err6:
	qvio_cdev_stop(&self->cdev);
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
	dma_blocks_free(self, pdev);
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
