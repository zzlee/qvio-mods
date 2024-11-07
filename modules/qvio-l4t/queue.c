#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "queue.h"
#include "video.h"

#include <linux/kernel.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

struct __queue_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list_ready;

	// vb2_buffer dma access
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

void qvio_queue_init(struct qvio_queue* self) {
	mutex_init(&self->queue_mutex);
	INIT_LIST_HEAD(&self->buffers);
	mutex_init(&self->buffers_mutex);
}

static int __queue_setup(struct vb2_queue *queue,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[]) {
	int err = 0;
	struct qvio_queue* self = container_of(queue, struct qvio_queue, queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct qvio_umods_req_entry* req_entry;
	int i;

	pr_info("+param %d %d\n", *num_buffers, *num_planes);

	if(*num_buffers < 1)
		*num_buffers = 1;

	switch(self->current_format.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		*num_planes = 1;
		switch(self->current_format.fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			sizes[0] = ALIGN(self->current_format.fmt.pix.width * 2, self->halign) *
				ALIGN(self->current_format.fmt.pix.height, self->valign);
			alloc_devs[0] = video->qdev->dev;
			break;

		case V4L2_PIX_FMT_NV12:
			sizes[0] = ALIGN(self->current_format.fmt.pix.width, self->halign) *
				ALIGN(self->current_format.fmt.pix.height, self->valign) * 3 / 2;
			alloc_devs[0] = video->qdev->dev;
			break;

		case V4L2_PIX_FMT_M420:
			sizes[0] = ALIGN(self->current_format.fmt.pix.width, self->halign) *
				ALIGN(self->current_format.fmt.pix.height * 3 / 2, self->valign);
			alloc_devs[0] = video->qdev->dev;
			break;

		default:
			pr_err("invalid value, self->current_format.fmt.pix.pixelformat=%d", (int)self->current_format.fmt.pix.pixelformat);
			err = -EINVAL;
			goto err0;
			break;
		}
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		switch(self->current_format.fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			*num_planes = 1;
			sizes[0] = ALIGN(self->current_format.fmt.pix_mp.width * 2, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign);
			alloc_devs[0] = video->qdev->dev;
			break;

		case V4L2_PIX_FMT_NV12:
			*num_planes = 2;
			sizes[0] = ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign);
			sizes[1] = ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign) / 2;
			alloc_devs[0] = video->qdev->dev;
			alloc_devs[1] = video->qdev->dev;
			break;

		case V4L2_PIX_FMT_M420:
			*num_planes = 1;
			sizes[0] = ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height * 3 / 2, self->valign);
			alloc_devs[0] = video->qdev->dev;
			break;

		default:
			pr_err("invalid value, self->current_format.fmt.pix_mp.pixelformat=%d", (int)self->current_format.fmt.pix_mp.pixelformat);
			err = -EINVAL;
			goto err0;
			break;
		}
		break;

	default:
		pr_err("unexpected value, self->current_format.type=%d\n", (int)self->current_format.type);
		err = -EINVAL;
		goto err0;
		break;
	}

	pr_info("-param %d %d [%d %d]\n", *num_buffers, *num_planes, sizes[0], sizes[1]);

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_QUEUE_SETUP;
	req_entry->req.u.queue_setup.num_buffers = *num_buffers;
	req_entry->req.u.queue_setup.num_planes = *num_planes;
	for(i = 0;i < (int)*num_planes;i++)
		req_entry->req.u.queue_setup.sizes[i] = sizes[i];
	qvio_umods_request(&video->umods, req_entry);

	return 0;

err1:
	kfree(req_entry);
err0:
	return err;
}

static int vmalloc_dma_map_sg(struct device* dev, void* vaddr, int size, struct sg_table* sgt, enum dma_data_direction dma_dir) {
	int err;
	int num_pages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct scatterlist *sg;
	int i, page_size;

#if 1 // DEBUG
	pr_info("-----vaddr=%p size=%d num_pages=%d\n", vaddr, size, num_pages);
#endif

	err = sg_alloc_table(sgt, num_pages, GFP_KERNEL);
	if (err) {
		pr_err("sg_alloc_table() failed, err=%d\n", err);
		goto err0;
	}
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		struct page *page = vmalloc_to_page(vaddr);

		if (!page) {
			err = -ENOMEM;
			goto err1;
		}
#if 1
		if(size > PAGE_SIZE) {
			page_size = PAGE_SIZE;
		} else {
			page_size = size;
		}
		sg_set_page(sg, page, page_size, 0);
		vaddr += page_size;
		size -= page_size;
#else
		sg_set_page(sg, page, PAGE_SIZE, 0);
		vaddr += PAGE_SIZE;
#endif
	}

#if 1
	sgt->nents = dma_map_sg(dev, sgt->sgl, sgt->orig_nents, dma_dir);
	if (!sgt->nents) {
		pr_err("dma_map_sg() failed\n");
		err = -EIO;
		goto err1;
	}
#endif

	return 0;

err1:
	sg_free_table(sgt);
err0:
	return err;
}

static void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u.\n", sgt, sgt->sgl, sgt->nents,
		sgt->orig_nents);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg)) {
		if(i > 4) {
			pr_info("... more pages ...\n");
			break;
		}

		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n", i, sg,
			sg_page(sg), sg->offset, sg->length, sg_dma_address(sg),
			sg_dma_len(sg));
	}
}

static int __buf_init(struct vb2_buffer *buffer) {
	int err;
	struct qvio_queue* self = vb2_get_drv_priv(buffer->vb2_queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buffer);
	struct __queue_buffer* buf = container_of(vbuf, struct __queue_buffer, vb);
	struct qvio_umods_req_entry* req_entry;
	int plane_size;
	void* vaddr;
	dma_addr_t dma_addr;

#if 1 // DEBUG
	pr_info("param: %p %p %d %p\n", self, vbuf, vbuf->vb2_buf.index, buf);
#endif

	plane_size = vb2_plane_size(buffer, 0);

	switch(buffer->memory) {
	case V4L2_MEMORY_MMAP:
		vaddr = vb2_plane_vaddr(buffer, 0);

		buf->dma_dir = DMA_NONE;
		err = vmalloc_dma_map_sg(video->qdev->dev, vaddr, plane_size, &buf->sgt, DMA_BIDIRECTIONAL);
		if(err) {
			pr_err("vmalloc_dma_map_sg() failed, err=%d\n", err);
			goto err0;
		}
		buf->dma_dir = DMA_BIDIRECTIONAL;

#if 1 // DEBUG
		pr_info("plane_size=%d, vaddr=%p\n", (int)plane_size, vaddr);
		sgt_dump(&buf->sgt);
#endif

		break;

	case V4L2_MEMORY_DMABUF:
		dma_addr = vb2_dma_contig_plane_dma_addr(buffer, 0);

#if 1 // DEBUG
		pr_info("plane_size=%d, dma_addr=0x%llx\n", (int)plane_size, dma_addr);
#endif
		break;

	default:
		pr_err("unexpected value, buffer->memory=%d\n", (int)buffer->memory);
		err = -EINVAL;
		goto err0;
		break;
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_BUF_INIT;
	req_entry->req.u.buf_init.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return 0;

err0:
	return err;
}

static void __buf_cleanup(struct vb2_buffer *buffer) {
	int err;
	struct qvio_queue* self = vb2_get_drv_priv(buffer->vb2_queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buffer);
	struct __queue_buffer* buf = container_of(vbuf, struct __queue_buffer, vb);
	struct sg_table* sgt = &buf->sgt;
	struct qvio_umods_req_entry* req_entry;

#if 1 // DEBUG
	pr_info("param: %p %p %d %p\n", self, vbuf, vbuf->vb2_buf.index, buf);
#endif

	switch(buffer->memory) {
	case V4L2_MEMORY_MMAP:
#if 1 // DEBUG
		sgt_dump(sgt);
#endif

		dma_unmap_sg(video->qdev->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
		sg_free_table(sgt);
		buf->dma_dir = DMA_NONE;
		break;

	case V4L2_MEMORY_DMABUF:
		break;

	default:
		pr_err("unexpected value, buffer->memory=%d\n", (int)buffer->memory);
		goto err0;
		break;
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_BUF_CLEANUP;
	req_entry->req.u.buf_cleanup.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return;

err0:
	return;
}

static int __buf_prepare(struct vb2_buffer *buffer) {
	int err;
	struct qvio_queue* self = vb2_get_drv_priv(buffer->vb2_queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buffer);
	struct __queue_buffer* buf = container_of(vbuf, struct __queue_buffer, vb);
	int plane_size;
	dma_addr_t dma_addr;
	struct qvio_umods_req_entry* req_entry;

#if 1 // DEBUG
	pr_info("param: %p %p %d %p\n", self, vbuf, vbuf->vb2_buf.index, buf);
#endif

	dma_addr = vb2_dma_contig_plane_dma_addr(buffer, 0);
	pr_info("dma_addr=0x%llx\n", dma_addr);

	switch(self->current_format.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		plane_size = vb2_plane_size(buffer, 0);
		switch(self->current_format.fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			if(plane_size < ALIGN(self->current_format.fmt.pix.width, self->halign) *
					ALIGN(self->current_format.fmt.pix.height, self->valign) * 2) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			break;

		case V4L2_PIX_FMT_NV12:
			if(plane_size < ALIGN(self->current_format.fmt.pix.width, self->halign) *
					ALIGN(self->current_format.fmt.pix.height, self->valign) * 3 / 2) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			break;

		case V4L2_PIX_FMT_M420:
			if(plane_size < ALIGN(self->current_format.fmt.pix.width, self->halign) *
					ALIGN(self->current_format.fmt.pix.height * 3 / 2, self->valign)) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			break;

		default:
			pr_err("unexpected value, self->current_format.fmt.pix.pixelformat=0x%X\n", (int)self->current_format.fmt.pix.pixelformat);

			err = -EINVAL;
			goto err0;
			break;
		}
		vb2_set_plane_payload(buffer, 0, plane_size);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		switch(self->current_format.fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			plane_size = vb2_plane_size(buffer, 0);
			if(plane_size < ALIGN(self->current_format.fmt.pix_mp.width * 2, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign)) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			vb2_set_plane_payload(buffer, 0, plane_size);
			break;

		case V4L2_PIX_FMT_NV12:
			plane_size = vb2_plane_size(buffer, 0);
			if(plane_size < ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign)) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			vb2_set_plane_payload(buffer, 0, plane_size);

			plane_size = vb2_plane_size(buffer, 1);
			if(plane_size < ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height, self->valign) / 2) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			vb2_set_plane_payload(buffer, 1, plane_size);
			break;

		case V4L2_PIX_FMT_M420:
			plane_size = vb2_plane_size(buffer, 0);
			if(plane_size < ALIGN(self->current_format.fmt.pix_mp.width, self->halign) *
				ALIGN(self->current_format.fmt.pix_mp.height * 3 / 2, self->valign)) {
				pr_err("unexpected value, plane_size=%d\n", plane_size);

				err = -EINVAL;
				goto err0;
			}
			vb2_set_plane_payload(buffer, 0, plane_size);
			break;

		default:
			pr_err("unexpected value, self->current_format.fmt.pix_mp.pixelformat=0x%X\n", (int)self->current_format.fmt.pix_mp.pixelformat);

			err = -EINVAL;
			goto err0;
			break;
		}
		break;

	default:
		pr_err("unexpected value, self->current_format.type=%d\n", (int)self->current_format.type);

		err = -EINVAL;
		goto err0;
		break;
	}

	if(vbuf->field == V4L2_FIELD_ANY)
		vbuf->field = V4L2_FIELD_NONE;

	switch(buffer->memory) {
	case V4L2_MEMORY_MMAP:
		dma_sync_sg_for_device(video->qdev->dev, buf->sgt.sgl, buf->sgt.orig_nents, DMA_BIDIRECTIONAL);
		break;

	case V4L2_MEMORY_DMABUF:
		break;

	default:
		pr_err("unexpected value, buffer->memory=%d\n", (int)buffer->memory);
		goto err0;
		break;
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_BUF_PREPARE;
	req_entry->req.u.buf_prepare.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return 0;

err0:
	return err;
}

static void __buf_finish(struct vb2_buffer *buffer) {
	int err;
	struct qvio_queue* self = vb2_get_drv_priv(buffer->vb2_queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buffer);
	struct __queue_buffer* buf = container_of(vbuf, struct __queue_buffer, vb);
	struct sg_table* sgt = &buf->sgt;
	struct qvio_umods_req_entry* req_entry;

#if 1 // DEBUG
	pr_info("param: %p %p %d %p\n", self, vbuf, vbuf->vb2_buf.index, buf);
#endif

	switch(buffer->memory) {
	case V4L2_MEMORY_MMAP:
		dma_sync_sg_for_cpu(video->qdev->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
		break;

	case V4L2_MEMORY_DMABUF:
		break;

	default:
		pr_err("unexpected value, buffer->memory=%d\n", (int)buffer->memory);
		goto err0;
		break;
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_BUF_FINISH;
	req_entry->req.u.buf_finish.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return;

err0:
	return;
}

static void __buf_queue(struct vb2_buffer *buffer) {
	int err;
	struct qvio_queue* self = vb2_get_drv_priv(buffer->vb2_queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buffer);
	struct __queue_buffer* buf = container_of(vbuf, struct __queue_buffer, vb);
	struct qvio_umods_req_entry* req_entry;

#if 0 // DEBUG
	pr_info("param: %p %p %d %p\n", self, vbuf, vbuf->vb2_buf.index, buf);
#endif

	if (!mutex_lock_interruptible(&self->buffers_mutex)) {
		list_add_tail(&buf->list_ready, &self->buffers);
		mutex_unlock(&self->buffers_mutex);
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_BUF_QUEUE;
	req_entry->req.u.buf_queue.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return;

err0:
	return;
}

static int __start_streaming(struct vb2_queue *queue, unsigned int count) {
	int err;
	struct qvio_queue* self = container_of(queue, struct qvio_queue, queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct qvio_device* qdev = video->qdev;
	struct __queue_buffer* buf;
	struct qvio_umods_req_entry* req_entry;
	ssize_t size;

	pr_info("count=%d\n", count);

	self->sequence = 0;

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_START_STREAMING;
	req_entry->req.u.buf_finish.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return 0;

err0:
	return -EIO;
}

static void __stop_streaming(struct vb2_queue *queue) {
	int err;
	struct qvio_queue* self = container_of(queue, struct qvio_queue, queue);
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct qvio_device* qdev = video->qdev;
	struct qvio_umods_req_entry* req_entry;

	pr_info("\n");

	if (!mutex_lock_interruptible(&self->buffers_mutex)) {
		struct __queue_buffer* buf;
		struct __queue_buffer* node;

		list_for_each_entry_safe(buf, node, &self->buffers, list_ready) {
#if 1 // DEBUG
			pr_info("vb2_buffer_done: %p %d\n", buf, buf->vb.vb2_buf.index);
#endif

			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
			list_del(&buf->list_ready);
		}

		mutex_unlock(&self->buffers_mutex);
	}

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_STOP_STREAMING;
	req_entry->req.u.buf_finish.flags = 0x1234;
	qvio_umods_request(&video->umods, req_entry);

	return;

err0:
	return;
}

static const struct vb2_ops qvio_vb2_ops = {
	.queue_setup     = __queue_setup,
	.buf_init        = __buf_init,
	.buf_cleanup     = __buf_cleanup,
	.buf_prepare     = __buf_prepare,
	.buf_finish      = __buf_finish,
	.buf_queue       = __buf_queue,
	.start_streaming = __start_streaming,
	.stop_streaming  = __stop_streaming,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
};

int qvio_queue_start(struct qvio_queue* self, enum v4l2_buf_type type) {
	pr_info("\n");

	self->queue.type = type;
	if(type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		self->queue.io_modes = VB2_READ;
	else
		self->queue.io_modes = VB2_WRITE;
	self->queue.io_modes |= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	self->queue.drv_priv = self;
	self->queue.lock = &self->queue_mutex;
	self->queue.buf_struct_size = sizeof(struct __queue_buffer);
	self->queue.mem_ops = &vb2_dma_contig_memops;
	self->queue.ops = &qvio_vb2_ops;
	self->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	self->queue.min_buffers_needed = 2;

	return 0;
}

void qvio_queue_stop(struct qvio_queue* self) {
	pr_info("\n");
}

struct vb2_queue* qvio_queue_get_vb2_queue(struct qvio_queue* self) {
	return &self->queue;
}

int qvio_queue_s_fmt(struct qvio_queue* self, struct v4l2_format *format) {
	int err;
	struct qvio_video* video = container_of(self, struct qvio_video, queue);
	struct qvio_umods_req_entry* req_entry;

	pr_info("\n");

	memcpy(&self->current_format, format, sizeof(struct v4l2_format));

	err = qvio_umods_req_entry_new(&req_entry);
	if(err) {
		pr_err("qvio_umods_req_entry_new() failed err=%d", err);
		goto err0;
	}

	req_entry->req.job_id = QVIO_UMODS_JOB_ID_S_FMT;
	memcpy(&req_entry->req.u.s_fmt.format, format, sizeof(struct v4l2_format));
	qvio_umods_request(&video->umods, req_entry);

	return 0;

err0:
	return err;
}

int qvio_queue_g_fmt(struct qvio_queue* self, struct v4l2_format *format) {
	pr_info("\n");

	memcpy(format, &self->current_format, sizeof(struct v4l2_format));

	return 0;
}

int qvio_queue_try_buf_done(struct qvio_queue* self) {
	int err;
	struct __queue_buffer* buf;

	pr_info("\n");

	err = mutex_lock_interruptible(&self->buffers_mutex);
	if (err) {
		pr_err("mutex_lock_interruptible() failed, err=%d\n", err);

		goto err0;
	}

	if (list_empty(&self->buffers)) {
		mutex_unlock(&self->buffers_mutex);
		pr_err("unexpected, list_empty()\n");

		goto err0;
	}

	buf = list_entry(self->buffers.next, struct __queue_buffer, list_ready);
	list_del(&buf->list_ready);
	mutex_unlock(&self->buffers_mutex);

	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);

	return 0;

err0:
	return err;
}
