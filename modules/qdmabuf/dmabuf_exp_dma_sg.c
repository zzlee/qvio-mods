#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "dmabuf_exp.h"

#include <linux/version.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/slab.h>

struct exp_dma_sg_buffer {
	struct device *dev;
	void *vaddr;
	unsigned long size;
	struct page **pages;
	struct sg_table sg_table;
	enum dma_data_direction dma_dir;

	struct dmabuf_exp_vmarea_handler handler;
	refcount_t refcount;
	struct sg_table *dma_sgt;
	unsigned int num_pages;
};

struct exp_dma_sg_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static void exp_dma_sg_buffer_put(void *buf_priv)
{
	struct exp_dma_sg_buffer *buf = buf_priv;
	struct sg_table *sgt = &buf->sg_table;
	int i = buf->num_pages;

	if (!refcount_dec_and_test(&buf->refcount))
		return;

	pr_info("Freeing buffer of %d pages\n", buf->num_pages);
	dma_unmap_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
		buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (buf->vaddr)
		vm_unmap_ram(buf->vaddr, buf->num_pages);
	sg_free_table(buf->dma_sgt);
	while (--i >= 0)
		__free_page(buf->pages[i]);
	kvfree(buf->pages);
	put_device(buf->dev);
	kfree(buf);
}

static int exp_dma_sg_attach(struct dma_buf *dbuf, struct dma_buf_attachment *dbuf_attach) {
	struct exp_dma_sg_attachment *attach;
	unsigned int i;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt;
	struct exp_dma_sg_buffer *buf = dbuf->priv;
	int ret;

	pr_info("buf=%p\n", buf);

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_err("kzalloc() failed\n");

		ret = -ENOMEM;
		goto err0;
	}

	sgt = &attach->sgt;
	/* Copy the buf->base_sgt scatter list to the attachment, as we can't
	 * map the same scatter list to multiple attachments at the same time.
	 */
	ret = sg_alloc_table(sgt, buf->dma_sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table() failed, err=%d\n", ret);

		goto err1;
	}

	pr_info("sgt->orig_nents=%d\n", sgt->orig_nents);

	rd = buf->dma_sgt->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;

err1:
	kfree(attach);
err0:
	return ret;
}

static void exp_dma_sg_detach(struct dma_buf *dbuf, struct dma_buf_attachment *db_attach) {
	struct exp_dma_sg_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	pr_info("attach=%p\n", attach);

	if (!attach) {
		pr_err("unexpected value, attach=%p\n", attach);
		goto err0;
	}

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE) {
		/*
		 * Cache sync can be skipped here, as the exporter memory is
		 * allocated from device coherent memory, which means the
		 * memory locations do not require any explicit cache
		 * maintenance prior or after being used by the device.
		 */
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents, attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	}
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;

	return;

err0:
	return;
}

static void exp_dma_sg_dma_buf_release(struct dma_buf *dbuf) {
	struct exp_dma_sg_buffer *buf = dbuf->priv;

	pr_info("buf=%p\n", buf);

	exp_dma_sg_buffer_put(dbuf->priv);
}

static struct sg_table * exp_dma_sg_map_dma_buf(struct dma_buf_attachment *db_attach,
	enum dma_data_direction dma_dir) {
	struct exp_dma_sg_attachment *attach = db_attach->priv;
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

static void exp_dma_sg_unmap_dma_buf(struct dma_buf_attachment * db_attach,
	struct sg_table * sgt,
	enum dma_data_direction dma_dir) {

	pr_info("db_attach=%p\n", db_attach);

	/* nothing to be done here */
}

static int exp_dma_sg_begin_cpu_access(struct dma_buf *dbuf,
	enum dma_data_direction direction)
{
	struct exp_dma_sg_buffer *buf = dbuf->priv;
	struct sg_table *sgt = buf->dma_sgt;

	pr_info("buf=%p\n", buf);

	dma_sync_sg_for_cpu(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);
	return 0;
}

static int exp_dma_sg_end_cpu_access(struct dma_buf *dbuf,
	enum dma_data_direction direction)
{
	struct exp_dma_sg_buffer *buf = dbuf->priv;
	struct sg_table *sgt = buf->dma_sgt;

	pr_info("buf=%p\n", buf);

	dma_sync_sg_for_device(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void * exp_dma_sg_vmap(struct dma_buf *dbuf)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,19,0)
static int exp_dma_sg_vmap(struct dma_buf *dbuf, struct dma_buf_map *map)
#else
static int exp_dma_sg_vmap(struct dma_buf *dbuf, struct iosys_map *map)
#endif
{
	struct exp_dma_sg_buffer *buf = dbuf->priv;

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

static int exp_dma_sg_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma) {
	struct exp_dma_sg_buffer *buf = dbuf->priv;
	int ret;

	pr_info("buf=%p\n", buf);

	if (!buf) {
		pr_err("unexpected value, buf=%p\n", buf);
		ret = -EINVAL;
		goto err0;
	}

	ret = vm_map_pages(vma, buf->pages, buf->num_pages);
	if (ret) {
		pr_err("vm_map_pages() failed, err=%d\n", ret);
		goto err0;
	}

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

static const struct dma_buf_ops exp_dma_sg_buf_ops = {
	.attach = exp_dma_sg_attach,
	.detach = exp_dma_sg_detach,
	.map_dma_buf = exp_dma_sg_map_dma_buf,
	.unmap_dma_buf = exp_dma_sg_unmap_dma_buf,
	.release = exp_dma_sg_dma_buf_release,
	.begin_cpu_access = exp_dma_sg_begin_cpu_access,
	.end_cpu_access = exp_dma_sg_end_cpu_access,
	.mmap = exp_dma_sg_mmap,
	.vmap = exp_dma_sg_vmap,
};

static int exp_dma_sg_alloc_compacted(struct exp_dma_sg_buffer *buf, gfp_t gfp_flags)
{
	unsigned int last_page = 0;
	unsigned long size = buf->size;

	while (size > 0) {
		struct page *pages;
		int order;
		int i;

		order = get_order(size);
		pr_info("order=%d, size=%d", (int)order, (int)size);
		/* Don't over allocate*/
		if ((PAGE_SIZE << order) > size)
			order--;

		pages = NULL;
		while (!pages) {
			pages = alloc_pages(GFP_KERNEL | __GFP_ZERO |
					__GFP_NOWARN | gfp_flags, order);
			if (pages)
				break;

			if (order == 0) {
				while (last_page--)
					__free_page(buf->pages[last_page]);
				return -ENOMEM;
			}
			order--;
		}

		split_page(pages, order);
		for (i = 0; i < (1 << order); i++)
			buf->pages[last_page++] = &pages[i];

		size -= PAGE_SIZE << order;
	}

	return 0;
}

int qdmabuf_dmabuf_alloc_dma_sg(struct device* device, int len, int fd_flags, int dma_dir) {
	struct exp_dma_sg_buffer *buf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	size_t size = PAGE_ALIGN(len);
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	int num_pages;
	int ret;

	pr_info("len=%d, fd_flags=%d\n", len, fd_flags);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		pr_err("kzalloc() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	buf->dev = get_device(device);
	if(!buf->dev) {
		pr_err("get_device() failed\n");
		ret = -EINVAL;
		goto err1;
	}

	buf->vaddr = NULL;
	buf->dma_dir = dma_dir;
	buf->size = size;
	/* size is already page aligned */
	buf->num_pages = size >> PAGE_SHIFT;
	buf->dma_sgt = &buf->sg_table;

	/*
	 * NOTE: dma-sg allocates memory using the page allocator directly, so
	 * there is no memory consistency guarantee, hence dma-sg ignores DMA
	 * attributes passed from the upper layer.
	 */
	buf->pages = kvmalloc_array(buf->num_pages, sizeof(struct page *), GFP_KERNEL | __GFP_ZERO);
	if (!buf->pages){
		pr_err("kvmalloc_array() failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	ret = exp_dma_sg_alloc_compacted(buf, GFP_DMA);
	if (ret) {
		pr_err("exp_dma_sg_alloc_compacted() failed, err=%d\n", ret);
		goto err3;
	}

	ret = sg_alloc_table_from_pages(buf->dma_sgt, buf->pages, buf->num_pages, 0, size, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed\n");
		goto err4;
	}

	sgt = &buf->sg_table;
	/*
	 * No need to sync to the device, this will happen later when the
	 * prepare() memop is called.
	 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5,10,120)
	sgt->nents = dma_map_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
				      buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (sgt->nents <= 0) {
		pr_err("dma_map_sg_attrs() failed\n");
		goto err5;
	}
#else
	ret = dma_map_sgtable(buf->dev, sgt, buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (ret) {
		pr_err("dma_map_sgtable() failed\n");
		goto err5;
	}
#endif

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = exp_dma_sg_buffer_put;
	buf->handler.arg = buf;
	refcount_set(&buf->refcount, 1);

	pr_info("%s: Allocated buffer of %d pages\n",
		__func__, buf->num_pages);

	exp_info.exp_name = "qdmabuf-dma-sg";
	exp_info.ops = &exp_dma_sg_buf_ops;
	exp_info.size = buf->size;
	exp_info.flags = fd_flags;
	exp_info.priv = buf;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_export() failed, dmabuf=%p\n", dmabuf);

		ret = PTR_ERR(dmabuf);
		goto err6;
	}

	ret = dma_buf_fd(dmabuf, fd_flags);
	if (ret < 0) {
		pr_err("dma_buf_fd() failed, err=%d\n", ret);

		goto err7;
	}

	return ret;

err7:
	dma_buf_put(dmabuf);
err6:
err5:
	sg_free_table(buf->dma_sgt);
err4:
	num_pages = buf->num_pages;
	while (num_pages--)
		__free_page(buf->pages[num_pages]);
err3:
	kvfree(buf->pages);
err2:
	put_device(buf->dev);
err1:
	kfree(buf);
err0:
	return ret;
}
