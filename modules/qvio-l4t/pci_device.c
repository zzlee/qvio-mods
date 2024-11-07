#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device.h"
#include "device.h"

#include <linux/version.h>
#include <linux/aer.h>
#include <linux/pci.h>

#define DRV_MODULE_NAME "qvio-pci"

static const struct pci_device_id __pci_ids[] = {
	{ PCI_DEVICE(0x12AB, 0xE380), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, __pci_ids);

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
		goto err1;
	}

	err = set_dma_mask(pdev);
	if(err) {
		pr_err("set_dma_mask() failed, err=%d\n", err);
		goto err2;
	}

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err2;
	}

	return 0;

err2:
	unmap_bars(self);
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

	qvio_cdev_stop(&self->cdev);
	unmap_bars(self);

	if (self->got_regions) {
		pr_info("pci_release_regions 0x%p.\n", pdev);
		pci_release_regions(pdev);
	}

	if (!self->regions_in_use) {
		pr_info("pci_disable_device 0x%p.\n", pdev);
		pci_disable_device(pdev);
	}

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
