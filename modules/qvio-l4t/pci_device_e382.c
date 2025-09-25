#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device_e382.h"
#include "utils.h"
#include "xdma_desc.h"

static irqreturn_t __irq_handler(int irq, void *dev_id);

int device_e382_probe(struct qvio_pci_device* self) {
	int err;

	err = qvio_xdma_wr_register();
	if(err) {
		pr_err("qvio_xdma_wr_register() failed\n");
		goto err0;
	}

	err = qvio_xdma_rd_register();
	if(err) {
		pr_err("qvio_xdma_rd_register() failed\n");
		goto err1;
	}

	err = qvio_tpg_register();
	if(err) {
		pr_err("qvio_tpg_register() failed\n");
		goto err2;
	}

	self->xdma_wr = qvio_xdma_wr_new();
	if(! self->xdma_wr) {
		pr_err("qvio_xdma_wr_new() failed\n");
		err = -ENOMEM;
		goto err3;
	}

	self->xdma_rd = qvio_xdma_rd_new();
	if(! self->xdma_rd) {
		pr_err("qvio_xdma_rd_new() failed\n");
		err = -ENOMEM;
		goto err4;
	}

	self->tpg = qvio_tpg_new();
	if(! self->tpg) {
		pr_err("qvio_tpg_new() failed\n");
		err = -ENOMEM;
		goto err5;
	}

	mutex_init(&self->mutex_xdma_irq_block);

	self->zdev->dev = self->dev;
	self->zdev->device_id = self->device_id;
	self->zdev->reg = (void __iomem *)((u64)self->bar[0] + 0x00000);
	err = qvio_zdev_probe(self->zdev);
	if(err) {
		pr_err("qvio_zdev_probe() failed, err=%d\n", err);
		goto err6;
	}

#if 1
	self->xdma_wr->dev = self->dev;
	self->xdma_wr->device_id = self->device_id;
	self->xdma_wr->reg = (void __iomem *)self->bar[1];
	self->xdma_wr->channel = 0;
	self->xdma_wr->mutex_irq_block = &self->mutex_xdma_irq_block;
	err = qvio_xdma_wr_probe(self->xdma_wr);
	if(err) {
		pr_err("qvio_xdma_wr_probe() failed, err=%d\n", err);
		goto err7;
	}

	self->xdma_rd->dev = self->dev;
	self->xdma_rd->device_id = self->device_id;
	self->xdma_rd->reg = (void __iomem *)self->bar[1];
	self->xdma_rd->channel = 0;
	self->xdma_rd->mutex_irq_block = &self->mutex_xdma_irq_block;
	err = qvio_xdma_rd_probe(self->xdma_rd);
	if(err) {
		pr_err("qvio_xdma_rd_probe() failed, err=%d\n", err);
		goto err8;
	}

	self->tpg->dev = self->dev;
	self->tpg->device_id = self->device_id;
	self->tpg->zdev = self->zdev;
	self->tpg->reg = (void __iomem *)((u64)self->bar[0] + 0x20000);
	self->tpg->reset_mask = BIT(0); // [x, x, tpg]
	err = qvio_tpg_probe(self->tpg);
	if(err) {
		pr_err("qvio_tpg_probe() failed, err=%d\n", err);
		goto err9;
	}

	self->qvio_axis_src = (void __iomem *)((u64)self->bar[0] + 0x10000);
	pr_info("reset axis_src...\n");
	err = qvio_zdev_reset_mask(self->zdev, BIT(1), 100); // [x, axis_src, x]
	if(err < 0) {
		pr_err("qvio_zdev_reset_mask() failed, err=%d\n", err);
		goto err10;
	}

	self->qvio_axis_sink = (void __iomem *)((u64)self->bar[0] + 0x11000);
	pr_info("reset axis_sink...\n");
	err = qvio_zdev_reset_mask(self->zdev, BIT(2), 100); // [axis_src, x, x]
	if(err < 0) {
		pr_err("qvio_zdev_reset_mask() failed, err=%d\n", err);
		goto err10;
	}
#endif

	return 0;

err10:
	qvio_tpg_remove(self->tpg);
err9:
	qvio_xdma_rd_remove(self->xdma_rd);
err8:
	qvio_xdma_wr_remove(self->xdma_wr);
err7:
	qvio_zdev_remove(self->zdev);
err6:
	qvio_tpg_put(self->tpg);
err5:
	qvio_xdma_rd_put(self->xdma_rd);
err4:
	qvio_xdma_wr_put(self->xdma_wr);
err3:
	qvio_tpg_unregister();
err2:
	qvio_xdma_rd_unregister();
err1:
	qvio_xdma_wr_unregister();
err0:
	return 0;
}

void device_e382_remove(struct qvio_pci_device* self) {
#if 1
	qvio_tpg_remove(self->tpg);
	qvio_xdma_rd_remove(self->xdma_rd);
	qvio_xdma_wr_remove(self->xdma_wr);
#endif
	qvio_zdev_remove(self->zdev);

	qvio_tpg_put(self->tpg);
	qvio_xdma_rd_put(self->xdma_rd);
	qvio_xdma_wr_put(self->xdma_wr);

	qvio_tpg_unregister();
	qvio_xdma_rd_unregister();
	qvio_xdma_wr_unregister();
}

int device_e382_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int err;
	int i;
	int irq_count;
	uintptr_t irq_block = (uintptr_t)((u64)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
	u32 vector;
	u32 value;

#if 0 // User Interrupt
	// IRQ Block User Interrupt Enable Mask
	io_write_reg(irq_block, 0x04, 0x0F); // Enable usr_irq_req[3:0]

	// IRQ Block User Vector Number
	value = io_read_reg(irq_block, 0x80);
	value = (value & ~(0x1F << 0)) | (0 << 0); // Map usr_irq_req[0] to MSI-X Vector 0
	value = (value & ~(0x1F << 8)) | (1 << 8); // Map usr_irq_req[1] to MSI-X Vector 1
	value = (value & ~(0x1F << 16)) | (2 << 16); // Map usr_irq_req[2] to MSI-X Vector 2
	value = (value & ~(0x1F << 24)) | (3 << 24); // Map usr_irq_req[3] to MSI-X Vector 3
	io_write_reg(irq_block, 0x80, value);
#endif

#if 1 // Channel Interupt
	if(self->msi_enabled) {
		irq_count = pci_msi_vec_count(pdev);

		if(irq_count >= 2) {
			irqreturn_t (*irq_handler_map[])(int irq, void *dev_id) = {
				qvio_xdma_rd_irq_handler,
				qvio_xdma_wr_irq_handler,
			};
			const char* irq_handler_name[] = {
				"xdma_rd",
				"xdma_wr",
			};
			void* irq_handler_dev[] = {
				self->xdma_rd,
				self->xdma_wr,
			};

			for(i = 0;i < 2;i++) {
				vector = pci_irq_vector(pdev, i);
				if(vector == -EINVAL) {
					err = -EINVAL;
					pr_err("pci_irq_vector() failed\n");
					goto err0;
				}

				pr_info("request_irq(%d, \"%s\", %p)\n", vector, irq_handler_name[i], irq_handler_dev[i]);
				err = request_irq(vector, irq_handler_map[i], 0, irq_handler_name[i], irq_handler_dev[i]);
				if(err) {
					pr_err("%d: request_irq(%u) failed, err=%d\n", i, vector, err);
					goto err0;
				}
				self->irq_lines[i] = vector;
			}

			// IRQ Block Channel Vector Number
			value = io_read_reg(irq_block, 0xA0);
			value = (value & ~(0x1F << 0)) | (0 << 0); // Map channel_int[0] to MSI-X Vector 0
			value = (value & ~(0x1F << 5)) | (1 << 5); // Map channel_int[1] to MSI-X Vector 1
			pr_info("channel_int=%X\n", value);
			io_write_reg(irq_block, 0xA0, value);
		} else if(irq_count >= 1) {
			i = 0;
			vector = pci_irq_vector(pdev, i);
			if(vector == -EINVAL) {
				err = -EINVAL;
				pr_err("pci_irq_vector() failed\n");
				goto err0;
			}

			pr_info("request_irq(%d, \"%s\", %p)\n", vector, QVIO_DRV_MODULE_NAME, self);
			err = request_irq(vector, __irq_handler, 0, QVIO_DRV_MODULE_NAME, self);
			if(err) {
				pr_err("%d: request_irq(%u) failed, err=%d\n", i, vector, err);
				goto err0;
			}
			self->irq_lines[i] = vector;

			// IRQ Block Channel Vector Number
			value = io_read_reg(irq_block, 0xA0);
			value = (value & ~(0x1F << 0)) | (0 << 0); // Map channel_int[0] to MSI-X Vector 0
			value = (value & ~(0x1F << 5)) | (0 << 5); // Map channel_int[1] to MSI-X Vector 0
			io_write_reg(irq_block, 0xA0, value);
		}
	}
#endif

	return 0;

err0:
	return err;
}

void device_e382_free_irqs(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int i;
	int irq_count;

	if(self->msi_enabled) {
		irq_count = pci_msi_vec_count(pdev);

		if(irq_count >= 2) {
			void* irq_handler_dev[] = {
				self->xdma_rd,
				self->xdma_wr,
			};

			for(i = 0;i < 2;i++) {
				if(self->irq_lines[i]) {
					pr_info("free_irq(%d, %p)\n", self->irq_lines[i], irq_handler_dev[i]);
					free_irq(self->irq_lines[i], irq_handler_dev[i]);
					self->irq_lines[i] = 0;
				}
			}
		} else if(irq_count >= 1) {
			i = 0;
			if(self->irq_lines[i]) {
				pr_info("free_irq(%d, %p)\n", self->irq_lines[i], self);
				free_irq(self->irq_lines[i], self);
				self->irq_lines[i] = 0;
			}
		}
	}
}

static irqreturn_t __irq_handler(int irq, void *dev_id) {
	struct qvio_pci_device* self = dev_id;
	irqreturn_t ret;

	ret = qvio_xdma_rd_irq_handler(irq, self->xdma_rd);
	if(ret == IRQ_NONE) {
		ret = qvio_xdma_wr_irq_handler(irq, self->xdma_wr);
	}

	return ret;
}
