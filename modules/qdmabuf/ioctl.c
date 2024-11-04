#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "ioctl.h"
#include "uapi/qdmabuf.h"
#include "dmabuf_exp.h"

#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>

static void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;
	dma_addr_t dma_addr;

	dma_addr = sg_dma_address(sg);

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u, dma_addr=%p.\n", sgt, sgt->sgl, sgt->nents,
		sgt->orig_nents, dma_addr);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg)) {
		if(i > 8) {
			pr_info("... more pages ...\n");
			break;
		}

		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n", i, sg,
			sg_page(sg), sg->offset, sg->length, sg_dma_address(sg),
			sg_dma_len(sg));
	}
}

long qdmabuf_ioctl_alloc(struct qdmabuf_device* device, unsigned long arg) {
	struct qdmabuf_alloc_args args;
	long ret = 0;
	struct device* dev = &device->pdev->dev;

	pr_info("\n");

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err;
	}

	switch(args.type) {
	case QDMABUF_TYPE_DMA_CONTIG:
		ret = qdmabuf_dmabuf_alloc_dma_contig(
			dev, args.len, args.fd_flags, args.dma_dir);
		break;

	case QDMABUF_TYPE_DMA_SG:
		ret = qdmabuf_dmabuf_alloc_dma_sg(
			dev, args.len, args.fd_flags, args.dma_dir);
		break;

	case QDMABUF_TYPE_VMALLOC:
		ret = qdmabuf_dmabuf_alloc_vmalloc(
			dev, args.len, args.fd_flags, args.dma_dir);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("qdmabuf_dmabuf_alloc_xxx() failed, err=%d\n", (int)ret);
		goto err;
	}

	args.fd = (__u32)ret;

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err;
	}

err:
	return ret;
}

long qdmabuf_ioctl_info(struct qdmabuf_device* device, unsigned long arg) {
	long ret;
	struct qdmabuf_info_args args;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct device* dev = &device->pdev->dev;

	pr_info("\n");

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	pr_info("fd=%d\n", args.fd);

	dmabuf = dma_buf_get(args.fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get() failed, dmabuf=%p\n", dmabuf);

		ret = -EINVAL;
		goto err0;
	}

	pr_info("dmabuf->size=%d\n", (int)dmabuf->size);

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_err("dma_buf_attach() failed, attach=%p\n", attach);

		ret = -EINVAL;
		goto err1;
	}

	pr_info("attach=%p\n", attach);

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
	if (IS_ERR(attach)) {
		pr_err("dma_buf_begin_cpu_access() failed, attach=%p\n", attach);
		goto err2;
	}

	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_err("dma_buf_map_attachment() failed, sgt=%p\n", sgt);

		ret = -EINVAL;
		goto err3;
	}

	sgt_dump(sgt);

	args.size = dmabuf->size;
	args.phy_addr = sg_dma_address(sgt->sgl);

	if (copy_to_user((void *)arg, &args, sizeof(args))) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err4;
	}

	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
	dma_buf_end_cpu_access(dmabuf, DMA_FROM_DEVICE);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);

	return 0;

err4:
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
err3:
	dma_buf_end_cpu_access(dmabuf, DMA_FROM_DEVICE);
err2:
	dma_buf_detach(dmabuf, attach);
err1:
	dma_buf_put(dmabuf);
err0:
	return ret;
}
