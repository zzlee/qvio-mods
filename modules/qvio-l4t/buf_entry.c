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

	switch(self->buf.buf_type) {
	case QVIO_BUF_TYPE_MMAP:
		dma_sync_sgtable_for_cpu(self->dev, self->u.userptr.sgt, self->dma_dir);
		dma_unmap_sg(self->dev, self->u.mmap.sgt->sgl, self->u.mmap.sgt->nents, self->dma_dir);
		sg_free_table(self->u.mmap.sgt);
		kfree(self->u.mmap.sgt);
		break;

	case QVIO_BUF_TYPE_USERPTR:
		dma_sync_sgtable_for_cpu(self->dev, self->u.userptr.sgt, self->dma_dir);
		// dma_unmap_sg(self->dev, self->u.userptr.sgt->sgl, self->u.userptr.sgt->nents, self->dma_dir);
		dma_unmap_sgtable(self->dev, self->u.userptr.sgt, self->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		sg_free_table(self->u.userptr.sgt);
		kfree(self->u.userptr.sgt);
		break;

	case QVIO_BUF_TYPE_DMABUF:
		if(self->u.dmabuf.dmabuf) {
			dma_buf_unmap_attachment(self->u.dmabuf.attach, self->u.dmabuf.sgt, self->dma_dir);
			dma_buf_end_cpu_access(self->u.dmabuf.dmabuf, self->dma_dir);
			dma_buf_detach(self->u.dmabuf.dmabuf, self->u.dmabuf.attach);
			dma_buf_put(self->u.dmabuf.dmabuf);
		} else {
			pr_err("unexpected value, self->u.dmabuf.dmabuf=%p", self->u.dmabuf.dmabuf);
		}
		break;

	default:
		pr_err("unexpected value, self->buf.buf_type=%d", self->buf.buf_type);
		break;
	}

	for(i = 0;i < ARRAY_SIZE(self->desc_blocks);i++) {
		if(! self->desc_blocks[i].dma_handle)
			continue;

		qvio_dma_block_free(&self->desc_blocks[i], self->desc_pool);
	}

	kfree(self);
}
