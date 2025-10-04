#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "pci_device_7024.h"
#include "utils.h"

static irqreturn_t __irq_handler(int irq, void *dev_id);

int device_7024_probe(struct qvio_pci_device* self) {
	int err;

	err = qvio_qdma_wr_register();
	if(err) {
		pr_err("qvio_qdma_wr_register() failed\n");
		goto err0;
	}

	err = qvio_qdma_rd_register();
	if(err) {
		pr_err("qvio_qdma_rd_register() failed\n");
		goto err1;
	}

	err = qvio_tpg_register();
	if(err) {
		pr_err("qvio_tpg_register() failed\n");
		goto err2;
	}

	self->qdma_wr_0 = qvio_qdma_wr_new();
	if(! self->qdma_wr_0) {
		pr_err("qvio_qdma_wr_new() failed\n");
		err = -ENOMEM;
		goto err3;
	}

	self->qdma_wr_1 = qvio_qdma_wr_new();
	if(! self->qdma_wr_1) {
		pr_err("qvio_qdma_wr_new() failed\n");
		err = -ENOMEM;
		goto err3_1;
	}

	self->qdma_rd = qvio_qdma_rd_new();
	if(! self->qdma_rd) {
		pr_err("qvio_qdma_rd_new() failed\n");
		err = -ENOMEM;
		goto err4;
	}

	self->tpg = qvio_tpg_new();
	if(! self->tpg) {
		pr_err("qvio_tpg_new() failed\n");
		err = -ENOMEM;
		goto err5;
	}

	self->reg_intr = (void __iomem *)((u64)self->bar[0] + 0x01000);

	self->zdev->dev = self->dev;
	self->zdev->device_id = self->device_id;
	self->zdev->reg = (void __iomem *)((u64)self->bar[0] + 0x00000);
	err = qvio_zdev_probe(self->zdev);
	if(err) {
		pr_err("qvio_zdev_probe() failed, err=%d\n", err);
		goto err6;
	}

#if 1
	self->qdma_wr_0->dev = self->dev;
	self->qdma_wr_0->device_id = self->device_id;
	self->qdma_wr_0->zdev = self->zdev;
	self->qdma_wr_0->reg = (void __iomem *)((u64)self->bar[0] + 0x10000);
	self->qdma_wr_0->reset_mask = BIT(0); // [x, x, x, qdma_wr_0]
	err = qvio_qdma_wr_probe(self->qdma_wr_0);
	if(err) {
		pr_err("qvio_qdma_wr_probe() failed, err=%d\n", err);
		goto err7;
	}

	self->qdma_wr_1->dev = self->dev;
	self->qdma_wr_1->device_id = self->device_id;
	self->qdma_wr_1->zdev = self->zdev;
	self->qdma_wr_1->reg = (void __iomem *)((u64)self->bar[0] + 0x12000);
	self->qdma_wr_1->reset_mask = BIT(3); // [qdma_wr_1, x, x, x]
	err = qvio_qdma_wr_probe(self->qdma_wr_1);
	if(err) {
		pr_err("qvio_qdma_wr_probe() failed, err=%d\n", err);
		goto err7_1;
	}

	self->qdma_rd->dev = self->dev;
	self->qdma_rd->device_id = self->device_id;
	self->qdma_rd->zdev = self->zdev;
	self->qdma_rd->reg = (void __iomem *)((u64)self->bar[0] + 0x11000);
	self->qdma_rd->reset_mask = BIT(1); // [x, x, qdma_rd, x]
	err = qvio_qdma_rd_probe(self->qdma_rd);
	if(err) {
		pr_err("qvio_qdma_rd_probe() failed, err=%d\n", err);
		goto err8;
	}

	self->tpg->dev = self->dev;
	self->tpg->device_id = self->device_id;
	self->tpg->zdev = self->zdev;
	self->tpg->reg = (void __iomem *)((u64)self->bar[0] + 0x20000);
	self->tpg->reset_mask = BIT(2); // [x, tpg, x, x]
	err = qvio_tpg_probe(self->tpg);
	if(err) {
		pr_err("qvio_qdma_wr_probe() failed, err=%d\n", err);
		goto err9;
	}
#endif

	return 0;

err9:
	qvio_qdma_rd_remove(self->qdma_rd);
err8:
	qvio_qdma_wr_remove(self->qdma_wr_1);
err7_1:
	qvio_qdma_wr_remove(self->qdma_wr_0);
err7:
	qvio_zdev_remove(self->zdev);
err6:
	qvio_tpg_put(self->tpg);
err5:
	qvio_qdma_rd_put(self->qdma_rd);
err4:
	qvio_qdma_wr_put(self->qdma_wr_1);
err3_1:
	qvio_qdma_wr_put(self->qdma_wr_0);
err3:
	qvio_tpg_unregister();
err2:
	qvio_qdma_rd_unregister();
err1:
	qvio_qdma_wr_unregister();
err0:
	return 0;
}

void device_7024_remove(struct qvio_pci_device* self) {
#if 1
	qvio_tpg_remove(self->tpg);
	qvio_qdma_rd_remove(self->qdma_rd);
	qvio_qdma_wr_remove(self->qdma_wr_1);
	qvio_qdma_wr_remove(self->qdma_wr_0);
#endif
	qvio_zdev_remove(self->zdev);

	qvio_tpg_put(self->tpg);
	qvio_qdma_rd_put(self->qdma_rd);
	qvio_qdma_wr_put(self->qdma_wr_1);
	qvio_qdma_wr_put(self->qdma_wr_0);

	qvio_tpg_unregister();
	qvio_qdma_rd_unregister();
	qvio_qdma_wr_unregister();
}

int device_7024_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int err;
	uintptr_t reg_intr = (uintptr_t)self->reg_intr;
	int i;
	int irq_count;
	u32 value;
	u32 vector;

#if 1
	if(self->msi_enabled) {
		irq_count = pci_msi_vec_count(pdev);

		if(irq_count >= 3) {
			irqreturn_t (*irq_handler_map[])(int irq, void *dev_id) = {
				qvio_qdma_wr_irq_handler,
				qvio_qdma_rd_irq_handler,
				qvio_qdma_wr_irq_handler,
			};
			const char* irq_handler_name[] = {
				"qdma_wr_0",
				"qdma_rd",
				"qdma_wr_1",
			};
			void* irq_handler_dev[] = {
				self->qdma_wr_0,
				self->qdma_rd,
				self->qdma_wr_1,
			};

			for(i = 0;i < 3;i++) {
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

			// IRQ Block User Vector Number
			value = io_read_reg(reg_intr, 0x00);
			value = (value & ~(0xFF << 0)) | (0 << 0); // Map usr_irq_req[0] to MSI-X Vector 0
			value = (value & ~(0xFF << 8)) | (1 << 8); // Map usr_irq_req[1] to MSI-X Vector 1
			value = (value & ~(0xFF << 16)) | (2 << 16); // Map usr_irq_req[2] to MSI-X Vector 2
			io_write_reg(reg_intr, 0x00, value);
		} else if(irq_count >= 1) { // aggregate irq handler
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

			// IRQ Block User Vector Number
			value = io_read_reg(reg_intr, 0x00);
			value = (value & ~(0xFF << 0)) | (0 << 0); // Map usr_irq_req[0] to MSI-X Vector 0
			value = (value & ~(0xFF << 8)) | (0 << 8); // Map usr_irq_req[1] to MSI-X Vector 0
			value = (value & ~(0xFF << 16)) | (0 << 16); // Map usr_irq_req[2] to MSI-X Vector 0
			io_write_reg(reg_intr, 0x00, value);
		} else {
			pr_err("irq_count=%d", irq_count);
			err = -EINVAL;
			goto err0;
		}
	}
#endif

	return 0;

err0:
	return err;
}

void device_7024_free_irqs(struct qvio_pci_device* self, struct pci_dev* pdev) {
	int i;
	int irq_count;

	if(self->msi_enabled) {
		irq_count = pci_msi_vec_count(pdev);

		if(irq_count >= 3) {
			void* irq_handler_dev[] = {
				self->qdma_wr_0,
				self->qdma_rd,
				self->qdma_wr_1,
			};

			for(i = 0;i < 3;i++) {
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

	ret = qvio_qdma_wr_irq_handler(irq, self->qdma_wr_0);
	if(ret != IRQ_NONE)
		return ret;

	ret = qvio_qdma_rd_irq_handler(irq, self->qdma_rd);
	if(ret != IRQ_NONE)
		return ret;

	ret = qvio_qdma_wr_irq_handler(irq, self->qdma_wr_1);
	if(ret != IRQ_NONE)
		return ret;

	return ret;
}