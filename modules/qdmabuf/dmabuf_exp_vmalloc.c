#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "dmabuf_exp.h"

#include <linux/version.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/slab.h>

struct exp_vmalloc_buffer {
	void *vaddr;
	unsigned long size;
	enum dma_data_direction dma_dir;

	struct dmabuf_exp_vmarea_handler handler;
	refcount_t refcount;
};

struct exp_vmalloc_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static void exp_vmalloc_buffer_put(void *buf_priv)
{
	struct exp_vmalloc_buffer *buf = buf_priv;

	if (!refcount_dec_and_test(&buf->refcount))
		return;

	vfree(buf->vaddr);
	kfree(buf);
}

static int exp_vmalloc_attach(struct dma_buf *dbuf, struct dma_buf_attachment *dbuf_attach) {
	struct exp_vmalloc_attachment *attach;
	struct exp_vmalloc_buffer *buf = dbuf->priv;
	int num_pages = PAGE_ALIGN(buf->size) / PAGE_SIZE;
	struct sg_table *sgt;
	struct scatterlist *sg;
	void *vaddr = buf->vaddr;
	int ret;
	int i;

	pr_info("buf=%p\n", buf);

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_err("kzalloc() failed\n");

		ret = -ENOMEM;
		goto err0;
	}

	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, num_pages, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table() failed, err=%d\n", ret);

		goto err1;
	}

	pr_info("sgt->orig_nents=%d\n", sgt->orig_nents);

	for_each_sg((sgt)->sgl, sg, (sgt)->nents, i) {
		struct page *page = vmalloc_to_page(vaddr);

		if (!page) {
			sg_free_table(sgt);
			kfree(attach);
			return -ENOMEM;
		}
		sg_set_page(sg, page, PAGE_SIZE, 0);
		vaddr += PAGE_SIZE;
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;

err1:
	kfree(attach);
err0:
	return ret;
}

static void exp_vmalloc_detach(struct dma_buf *dbuf, struct dma_buf_attachment *db_attach) {
	struct exp_vmalloc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	pr_info("attach=%p\n", attach);

	if (!attach) {
		pr_err("unexpected value, attach=%p\n", attach);
		goto err0;
	}

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,10,120)
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents,
				   attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
#else
		dma_unmap_sgtable(db_attach->dev, sgt, attach->dma_dir, 0);
#endif
	}
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;

	return;

err0:
	return;
}

static void exp_vmalloc_dma_buf_release(struct dma_buf *dbuf) {
	struct exp_vmalloc_buffer *buf = dbuf->priv;

	pr_info("buf=%p\n", buf);

	exp_vmalloc_buffer_put(dbuf->priv);
}

static struct sg_table * exp_vmalloc_map_dma_buf(struct dma_buf_attachment *db_attach, enum dma_data_direction dma_dir) {
	struct exp_vmalloc_attachment *attach = db_attach->priv;
	/* stealing dmabuf mutex to serialize map/unmap operations */
	// struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,10,120)
#else
	int err;
#endif

	pr_info("db_attach=%p\n", db_attach);

	if (!attach) {
		pr_err("unexpected value, attach=%p\n", attach);

		sgt = NULL;
		goto err0;
	}

	// mutex_lock(lock);

	sgt = &attach->sgt;
	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir) {
		goto done;
	}

	/* release any previous cache */
	if (attach->dma_dir != DMA_NONE) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,10,120)
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents,
				   attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
#else
		dma_unmap_sgtable(db_attach->dev, sgt, attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
#endif
		attach->dma_dir = DMA_NONE;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,10,120)
	sgt->nents = dma_map_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (!sgt->nents) {
		pr_err("dma_map_sg_attrs() failed, sgt->nents=%d\n", (int)sgt->nents);
		sgt = ERR_PTR(-EIO);
		goto err1;
	}
#else
	err = dma_map_sgtable(db_attach->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if(err) {
		pr_err("dma_map_sgtable() failed, err=%d\n", err);
		sgt = ERR_PTR(-EIO);
		goto err1;
	}
#endif

	attach->dma_dir = dma_dir;

done:
	// mutex_unlock(lock);
	return sgt;

err1:
	// mutex_unlock(lock);
err0:
	return sgt;
}

static void exp_vmalloc_unmap_dma_buf(struct dma_buf_attachment * db_attach, struct sg_table * sgt, enum dma_data_direction dma_dir) {
	pr_info("db_attach=%p\n", db_attach);

	/* nothing to be done here */
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void * exp_vmalloc_vmap(struct dma_buf *dbuf)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,19,0)
static int exp_vmalloc_vmap(struct dma_buf *dbuf, struct dma_buf_map *map)
#else
static int exp_vmalloc_vmap(struct dma_buf *dbuf, struct iosys_map *map)
#endif
{
	struct exp_vmalloc_buffer *buf = dbuf->priv;

	pr_info("buf=%p\n", buf);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
	return buf->vaddr;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,18,0)
	dma_buf_map_set_vaddr(map, buf->vaddr);

	return 0;
#else
	iosys_map_set_vaddr(map, buf->vaddr);

	return 0;
#endif
}

static int exp_vmalloc_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma) {
	struct exp_vmalloc_buffer *buf = dbuf->priv;
	int ret;

	pr_info("buf=%p\n", buf);

	if (!buf) {
		pr_err("unexpected value, buf=%p\n", buf);
		ret = -EINVAL;
		goto err0;
	}

	ret = remap_vmalloc_range(vma, buf->vaddr, 0);
	if (ret) {
		pr_err("remap_vmalloc_range() failed, err=%d\n", ret);
		goto err0;
	}

	/*
	 * Make sure that vm_areas for 2 buffers won't be merged together
	 */
#if KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE
	vma->__vm_flags |= VM_DONTEXPAND;
#else
	vma->vm_flags |= VM_DONTEXPAND;
#endif
	/*
	 * Use common vm_area operations to track buffer refcount.
	 */
	vma->vm_private_data = &buf->handler;
	vma->vm_ops = &dmabuf_exp_vm_ops;

	vma->vm_ops->open(vma);

	return 0;

err0:
	return ret;
}

static const struct dma_buf_ops exp_vmalloc_buf_ops = {
	.attach = exp_vmalloc_attach,
	.detach = exp_vmalloc_detach,
	.map_dma_buf = exp_vmalloc_map_dma_buf,
	.unmap_dma_buf = exp_vmalloc_unmap_dma_buf,
	.release = exp_vmalloc_dma_buf_release,
	.mmap = exp_vmalloc_mmap,
	.vmap = exp_vmalloc_vmap,
};

int qdmabuf_dmabuf_alloc_vmalloc(struct device* device, int len, int fd_flags, int dma_dir) {
	struct exp_vmalloc_buffer *buf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	size_t size = PAGE_ALIGN(len);
	struct dma_buf *dmabuf;
	int ret;

	pr_info("len=%d, fd_flags=%d\n", len, fd_flags);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		pr_err("kzalloc() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	buf->size = size;
	buf->vaddr = vmalloc_user(buf->size);
	if (!buf->vaddr) {
		pr_err("vmalloc_user() failed\n");
		ret = -ENOMEM;
		goto err1;
	}

	buf->dma_dir = dma_dir;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = exp_vmalloc_buffer_put;
	buf->handler.arg = buf;
	refcount_set(&buf->refcount, 1);

	exp_info.exp_name = "qdmabuf-vmallloc";
	exp_info.ops = &exp_vmalloc_buf_ops;
	exp_info.size = buf->size;
	exp_info.flags = fd_flags;
	exp_info.priv = buf;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_export() failed, dmabuf=%p\n", dmabuf);

		ret = PTR_ERR(dmabuf);
		goto err2;
	}

	ret = dma_buf_fd(dmabuf, fd_flags);
	if (ret < 0) {
		pr_err("dma_buf_fd() failed, err=%d\n", ret);

		goto err3;
	}

	return ret;

err3:
	dma_buf_put(dmabuf);
err2:
	vfree(buf->vaddr);
err1:
	kfree(buf);
err0:
	return ret;
}
