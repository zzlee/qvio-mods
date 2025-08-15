#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "utils.h"

ssize_t utils_calc_buf_size(struct qvio_format* format, __u32 offset[4], __u32 stride[4]) {
	ssize_t ret;

	switch(format->fmt) {
	case FOURCC_Y800:
		ret = (ssize_t)(offset[0] + stride[0] * format->height);
		break;

	case FOURCC_NV16: // notice! offset[1] covers plane-0
		ret = (ssize_t)(offset[1] + stride[1] * format->height);
		break;

	case FOURCC_NV12: // notice! offset[1] covers plane-0
		ret = (ssize_t)(offset[1] + stride[1] * (format->height >> 1));
		break;

	default:
		pr_err("unexpected value, format->fmt=%08X\n", format->fmt);
		ret = -EINVAL;
		goto err0;
		break;
	}

	return ret;

err0:
	return ret;
}

void utils_sgt_dump(struct sg_table *sgt, bool full)
{
	int i;
	struct scatterlist *sg = sgt->sgl;
	dma_addr_t dma_addr;

	dma_addr = sg_dma_address(sg);

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u, dma_addr=%p. (PAGE_SIZE=%lu, PAGE_MASK=%lX)\n", sgt, sgt->sgl, sgt->nents,
		sgt->orig_nents, (void*)dma_addr, PAGE_SIZE, PAGE_MASK);

	for (i = 0; i < sgt->nents; i++, sg = sg_next(sg)) {
		if(! full && i > 8) {
			pr_info("... more pages ...\n");
			break;
		}

		if((sg_dma_len(sg) & ~PAGE_MASK)) {
			pr_err("unexpected dma_size!! %lX\n", (sg_dma_len(sg) & ~PAGE_MASK));
		}

		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n", i, sg,
			sg_page(sg), sg->offset, sg->length, sg_dma_address(sg),
			sg_dma_len(sg));
	}
}
