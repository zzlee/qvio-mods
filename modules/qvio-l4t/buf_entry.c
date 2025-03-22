#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "buf_entry.h"

#include <linux/kernel.h>
#include <linux/slab.h>

static void __buf_entry_free(struct kref *ref);

struct qvio_buf_entry* qvio_buf_entry_new(void) {
	struct qvio_buf_entry* self;

	self = kzalloc(sizeof(struct qvio_buf_entry), GFP_KERNEL);
	if(! self) {
		pr_err("out of memory\n");
		goto err0;
	}

	kref_init(&self->ref);
	INIT_LIST_HEAD(&self->node);

	// pr_info("self=%px\n", self);

	return self;

err0:
	return self;
}

struct qvio_buf_entry* qvio_buf_entry_get(struct qvio_buf_entry* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

void qvio_buf_entry_put(struct qvio_buf_entry* self) {
	if (self)
		kref_put(&self->ref, __buf_entry_free);
}

static void __buf_entry_free(struct kref *ref) {
	struct qvio_buf_entry* self = container_of(ref, struct qvio_buf_entry, ref);
	int i;

	// pr_info("self=%px\n", self);

	switch(self->qbuf.buf_type) {
	case 0:
		break;

	case 1:
		dma_unmap_sg(self->dev, self->sgt_userptr.sgl, self->sgt_userptr.nents, self->dma_dir);
		sg_free_table(&self->sgt_userptr);
		break;

	case 2:
		if(self->dmabuf) {
			dma_buf_unmap_attachment(self->attach, self->sgt, self->dma_dir);
			dma_buf_end_cpu_access(self->dmabuf, self->dma_dir);
			dma_buf_detach(self->dmabuf, self->attach);
			dma_buf_put(self->dmabuf);
		} else {
			pr_err("unexpected value, self->dmabuf=%p", self->dmabuf);
		}
		break;

	default:
		pr_err("unexpected value, self->qbuf.buf_type=%d", self->qbuf.buf_type);
		break;
	}

	for(i = 0;i < ARRAY_SIZE(self->desc_blocks);i++) {
		if(! self->desc_blocks[i].dma_handle)
			continue;

		qvio_dma_block_free(&self->desc_blocks[i], self->desc_pool);
	}

	kfree(self);
}
