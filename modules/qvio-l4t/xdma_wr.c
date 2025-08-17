static int start_buf_entry(struct qvio_device* self, struct qvio_buf_entry* buf_entry) {
	u32* xdma_irq_block;
	u32* xdma_c2h_channel;
	u32* xdma_c2h_sgdma;

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
}

static irqreturn_t __irq_handler_xdma(int irq, void *dev_id)
{
	struct qvio_device* self = dev_id;
	u32* zzlab_env;
	u32* xdma_irq_block;
	u32* xdma_c2h_channel;
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
