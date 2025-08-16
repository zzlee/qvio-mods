#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/poll.h>

#include "video_queue.h"
#include "utils.h"

static void __free(struct kref *ref);
static long __file_ioctl_s_fmt(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_g_fmt(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_req_bufs(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_query_buf(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_qbuf(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_dqbuf(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_streamon(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_streamoff(struct qvio_video_queue* self, struct file * filp, unsigned long arg);
static long __file_ioctl_qbuf_userptr(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf);
static long __file_ioctl_qbuf_dmabuf(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf);
static long __file_ioctl_qbuf_mmap(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf);
static int qbuf_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry);

struct qvio_video_queue* qvio_video_queue_new(void) {
	int err;
	struct qvio_video_queue* self = kzalloc(sizeof(struct qvio_video_queue), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	spin_lock_init(&self->lock);
	INIT_LIST_HEAD(&self->job_list);
	INIT_LIST_HEAD(&self->done_list);
	self->state = QVIO_VIDEO_QUEUE_STATE_READY;

	init_waitqueue_head(&self->irq_wait);

	return self;

err0:
	return NULL;
}

struct qvio_video_queue* qvio_video_queue_get(struct qvio_video_queue* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __free(struct kref *ref) {
	struct qvio_video_queue* self = container_of(ref, struct qvio_video_queue, ref);

	// pr_info("\n");

	kfree(self);
}

void qvio_video_queue_put(struct qvio_video_queue* self) {
	if (self)
		kref_put(&self->ref, __free);
}

__poll_t qvio_video_queue_file_poll(struct qvio_video_queue* self, struct file *filp, struct poll_table_struct *wait) {
	poll_wait(filp, &self->irq_wait, wait);

	if (! list_empty(&self->done_list))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

long qvio_video_queue_file_ioctl(struct qvio_video_queue* self, struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;

	switch(cmd) {
	case QVIO_IOC_S_FMT:
		ret = __file_ioctl_s_fmt(self, filp, arg);
		break;

	case QVIO_IOC_G_FMT:
		ret = __file_ioctl_g_fmt(self, filp, arg);
		break;

	case QVIO_IOC_REQ_BUFS:
		ret = __file_ioctl_req_bufs(self, filp, arg);
		break;

	case QVIO_IOC_QUERY_BUF:
		ret = __file_ioctl_query_buf(self, filp, arg);
		break;

	case QVIO_IOC_QBUF:
		ret = __file_ioctl_qbuf(self, filp, arg);
		break;

	case QVIO_IOC_DQBUF:
		ret = __file_ioctl_dqbuf(self, filp, arg);
		break;

	case QVIO_IOC_STREAMON:
		ret = __file_ioctl_streamon(self, filp, arg);
		break;

	case QVIO_IOC_STREAMOFF:
		ret = __file_ioctl_streamoff(self, filp, arg);
		break;

	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

static long __file_ioctl_s_fmt(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_format args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	switch(args.fmt) {
	case FOURCC_Y800:
		self->planes = 1;
		break;

	case FOURCC_NV12:
	case FOURCC_NV16:
		self->planes = 2;
		break;

	default:
		self->planes = 0;

		ret = -EINVAL;
		goto err0;
		break;
	}

	self->format = args;
	pr_info("%dx%d 0x%08X\n", self->format.width, self->format.height, self->format.fmt);

	return 0;

err0:
	return ret;
}

static long __file_ioctl_g_fmt(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_format args;

	args = self->format;

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_req_bufs(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_req_bufs args;
	ssize_t buf_size;
	int i;
	__u32 offset;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	if(self->buffers) {
		kfree(self->buffers);
		self->buffers_count = 0;
	}

	self->buffers = kcalloc(args.count, sizeof(struct qvio_buffer), GFP_KERNEL);
	if(! self->buffers) {
		pr_err("kcalloc() failed\n");
		goto err0;
	}
	self->buffers_count = args.count;

	for(i = 0;i < args.count;i++) {
		self->buffers[i].index = i;
		self->buffers[i].buf_type = args.buf_type;
		memcpy(self->buffers[i].offset, args.offset, ARRAY_SIZE(args.offset));
		memcpy(self->buffers[i].stride, args.stride, ARRAY_SIZE(args.stride));
	}

	buf_size = utils_calc_buf_size(&self->format, args.offset, args.stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}
	pr_info("buf_size=%lu\n", buf_size);

	self->buffer_size = buf_size;

	switch(args.buf_type) {
	case QVIO_BUF_TYPE_MMAP:
		if(self->mmap_buffer) vfree(self->mmap_buffer);

		self->mmap_buffer_size = (size_t)(buf_size * args.count);
		self->mmap_buffer = vmalloc(self->mmap_buffer_size);
		if(! self->mmap_buffer) {
			pr_err("vmalloc() failed\n");
			goto err0;
		}

		offset = 0;
		for(i = 0;i < args.count;i++) {
			self->buffers[i].u.offset = offset;

			offset += buf_size;
		}

		break;

#if 0
	case QVIO_BUF_TYPE_DMABUF:
		break;

	case QVIO_BUF_TYPE_USERPTR:
		break;
#endif

	default:
		pr_err("unexpected value, args.buf_type=%d\n", (int)args.buf_type);
		ret = -EINVAL;
		goto err0;
		break;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_query_buf(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_buffer args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	if(args.index >= self->buffers_count) {
		pr_err("unexpected value, %u >= %u\n", args.index, self->buffers_count);

		ret = -EINVAL;
		goto err0;
	}

	args = self->buffers[args.index];

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_qbuf(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_buffer buf;

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	switch(buf.buf_type) {
	case QVIO_BUF_TYPE_USERPTR:
		ret = __file_ioctl_qbuf_userptr(self, filp, &buf);
		break;

	case QVIO_BUF_TYPE_DMABUF:
		ret = __file_ioctl_qbuf_dmabuf(self, filp, &buf);
		break;

	case QVIO_BUF_TYPE_MMAP:
		ret = __file_ioctl_qbuf_mmap(self, filp, &buf);
		break;

	default:
		pr_err("unexpected value, buf.buf_type=%d\n", buf.buf_type);
		ret = -EINVAL;
		break;
	}

	return ret;

err0:
	return ret;
}

static long __file_ioctl_dqbuf(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	unsigned long flags;
	struct qvio_buf_entry* buf_entry;
	struct qvio_buffer args;

	spin_lock_irqsave(&self->lock, flags);
	if(list_empty(&self->done_list)) {
		spin_unlock_irqrestore(&self->lock, flags);

		ret = -EAGAIN;
		goto err0;
	}

	buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
	list_del(&buf_entry->node);
	spin_unlock_irqrestore(&self->lock, flags);

	// pr_info("buf_entry->buf.index=0x%llX\n", (int64_t)buf_entry->buf.index);
	args.index = buf_entry->buf.index;

	qvio_buf_entry_put(buf_entry);

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamon(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	int err;
	struct qvio_buf_entry* buf_entry;
#if 0
	struct qvio_device* self = filp->private_data;
	struct pci_dev* pdev = self->pci_dev;
	uintptr_t zzlab_env = self->zzlab_env;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	XV_tpg* xTpg = &self->xTpg;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	uintptr_t qdma_wr;
	uintptr_t qdma_rd;
	long ret;
	struct qvio_buf_entry* buf_entry;
	u32 value;
	u32 nIsIdle;
	int i;
	int reset_mask;

	if(self->state == QVIO_VIDEO_QUEUE_STATE_START) {
		pr_err("unexpected value, self->state=%d\n", self->state);
		ret = -EINVAL;
		goto err0;
	}

	switch(self->work_mode) {
	case QVIO_WORK_MODE_XDMA_H2C:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("H2C Channel Identifier: 0x%08X\n", xdma_h2c_channel[0x00 >> 2]);
		pr_info("H2C Channel Status: 0x%X\n", xdma_h2c_channel[0x40 >> 2]);
		pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x14 >> 2] = 0x01; // W1S engine_int_req[0:0]
		xdma_h2c_channel[0x04 >> 2] = 0;
#endif
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("C2H Channel Identifier: 0x%08X\n", xdma_c2h_channel[0x00 >> 2]);
		pr_info("C2H Channel Status: 0x%X\n", xdma_c2h_channel[0x40 >> 2]);
		pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x14 >> 2] = 0x02; // W1S engine_int_req[1:1]
		xdma_c2h_channel[0x04 >> 2] = 0;
#endif
		break;

	case QVIO_WORK_MODE_QDMA_WR:
		switch(pdev->device) {
		case 0x7024:
			reset_mask = 0x01; // [x, x, qdma_wr]
			break;

		default:
			reset_mask = 0;
			pr_err("unexpected value, pdev->device=0x%04X\n", (int)pdev->device);
			break;
		}

		pr_info("reset qdma_wr...\n");
		value = io_read_reg(zzlab_env, 0x10);
		io_write_reg(zzlab_env, 0x10, value & ~reset_mask);
		msleep(100);
		io_write_reg(zzlab_env, 0x10, value | reset_mask);

		qdma_wr = (uintptr_t)self->qdma_wr;
		pr_info("qdma_wr[0x00]=0x%x\n", io_read_reg(qdma_wr, 0x00));
		io_write_reg(qdma_wr, 0x00, 0x00);
		io_write_reg(qdma_wr, 0x04, 0x01); // GIE
		io_write_reg(qdma_wr, 0x08, 0x01); // IER (ap_done)
		break;

	case QVIO_WORK_MODE_QDMA_RD:
		switch(pdev->device) {
		case 0x7024:
			reset_mask = 0x02; // [x, qdma_rd, x]
			break;

		default:
			reset_mask = 0;
			pr_err("unexpected value, pdev->device=0x%04X\n", (int)pdev->device);
			break;
		}

		pr_info("reset qdma_rd...\n");
		value = io_read_reg(zzlab_env, 0x10);
		io_write_reg(zzlab_env, 0x10, value & ~reset_mask);
		msleep(100);
		io_write_reg(zzlab_env, 0x10, value | reset_mask);

		qdma_rd = (uintptr_t)self->qdma_rd;
		pr_info("qdma_rd[0x00]=0x%x\n", io_read_reg(qdma_rd, 0x00));
		io_write_reg(qdma_rd, 0x00, 0x00);
		io_write_reg(qdma_rd, 0x04, 0x01); // GIE
		io_write_reg(qdma_rd, 0x08, 0x01); // IER (ap_done)
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = -EINVAL;
		goto err0;
		break;
	}
#endif
	if(self->state == QVIO_VIDEO_QUEUE_STATE_START) {
		pr_err("unexpected value, self->state=%d\n", self->state);
		ret = -EINVAL;
		goto err0;
	}

	err = self->streamon(self);
	if(err) {
		pr_err("streamon() failed, err=%d\n", err);
		ret = err;
		goto err0;
	}

	if(! list_empty(&self->job_list)) {
		buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
		err = self->start_buf_entry(self, buf_entry);
		if(err) {
			pr_err("self->start_buf_entry() failed, err=%d\n", err);
		}
	}

	self->state = QVIO_VIDEO_QUEUE_STATE_START;

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamoff(struct qvio_video_queue* self, struct file * filp, unsigned long arg) {
	long ret;
	int err;
	unsigned long flags;
	struct qvio_buf_entry* buf_entry;
#if 0
	struct qvio_device* self = filp->private_data;
	struct pci_dev* pdev = self->pci_dev;
	uintptr_t zzlab_env = (uintptr_t)self->zzlab_env;
	XAximm_test0* xaximm_test0 = &self->xaximm_test0;
	XAximm_test1* xaximm_test1 = &self->xaximm_test1;
	XZ_frmbuf_writer* xFrmBufWr = &self->xFrmBufWr;
	XV_tpg* xTpg = &self->xTpg;
	XAximm_test0* xaximm_test2 = &self->xaximm_test2;
	XAximm_test0* xaximm_test3 = &self->xaximm_test3;
	XAximm_test0* xaximm_test2_1 = &self->xaximm_test2_1;
	uintptr_t qdma_wr;
	uintptr_t qdma_rd;
	u32* xdma_irq_block;
	u32* xdma_h2c_channel;
	u32* xdma_c2h_channel;
	u32 value;
	u32 nIsIdle;
	int i;
	unsigned long flags;
	struct qvio_buf_entry* buf_entry;
	int reset_mask;

	switch(self->work_mode) {
	case QVIO_WORK_MODE_XDMA_H2C:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_h2c_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x0, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("H2C Channel Identifier: 0x%08X\n", xdma_h2c_channel[0x00 >> 2]);
		pr_info("H2C Channel Status: 0x%X\n", xdma_h2c_channel[0x40 >> 2]);
		pr_info("H2C Channel Completed Descriptor Count: %d\n", xdma_h2c_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x18 >> 2] = 0x01; // W1C engine_int_req[0:0]
		xdma_h2c_channel[0x04 >> 2] = 0;
#endif
		break;

	case QVIO_WORK_MODE_XDMA_C2H:
		xdma_irq_block = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x2, 0, 0));
		xdma_c2h_channel = (u32*)((u8*)self->bar[1] + xdma_mkaddr(0x1, 0, 0));

		pr_info("IRQ Block Identifier: 0x%08X\n", xdma_irq_block[0x00 >> 2]);
		pr_info("C2H Channel Identifier: 0x%08X\n", xdma_c2h_channel[0x00 >> 2]);
		pr_info("C2H Channel Status: 0x%X\n", xdma_c2h_channel[0x40 >> 2]);
		pr_info("C2H Channel Completed Descriptor Count: %d\n", xdma_c2h_channel[0x48 >> 2]);

#if 1
		xdma_irq_block[0x18 >> 2] = 0x02; // W1C engine_int_req[1:1]
		xdma_c2h_channel[0x04 >> 2] = 0;
#endif
		break;

	default:
		pr_err("unexpected value, self->work_mode=%d\n", self->work_mode);
		ret = -EINVAL;
		goto err0;
		break;
	}
#endif
	if(self->state == QVIO_VIDEO_QUEUE_STATE_READY) {
		pr_err("unexpected value, self->state=%d\n", self->state);
		ret = -EINVAL;
		goto err0;
	}

	err = self->streamoff(self);
	if(err) {
		pr_err("streamoff() failed, err=%d\n", err);
		ret = err;
		goto err0;
	}

	spin_lock_irqsave(&self->lock, flags);
	if(! list_empty(&self->job_list)) {
		pr_warn("job list is not empty!!\n");

		while(! list_empty(&self->job_list)) {
			buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}

	if(! list_empty(&self->done_list)) {
		pr_warn("done list is not empty!!\n");

		while(! list_empty(&self->done_list)) {
			buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}
	spin_unlock_irqrestore(&self->lock, flags);

	self->state = QVIO_VIDEO_QUEUE_STATE_READY;

	return 0;

err0:
	return ret;
}

static long __file_ioctl_qbuf_userptr(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf) {
	long ret;
	int err;
	ssize_t buf_size;
	unsigned long start;
	unsigned long length;
	unsigned long first, last;
	unsigned long nr;
	struct frame_vector *vec;
	unsigned int flags_vec = FOLL_FORCE | FOLL_WRITE;
	int nr_pages;
	struct sg_table* sgt;
	enum dma_data_direction dma_dir = buf->buf_dir;
	struct qvio_buf_entry* buf_entry;

	start = buf->u.userptr;

	buf_size = utils_calc_buf_size(&self->format, buf->offset, buf->stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}
	// pr_info("buf_size=%lu\n", buf_size);

	length = buf_size;
	first = start >> PAGE_SHIFT;
	last = (start + length - 1) >> PAGE_SHIFT;
	nr = last - first + 1;
	// pr_info("start=%lX, length=%lu, nr=%lu\n", start, length, nr);

	vec = frame_vector_create(nr);
	if(! vec) {
		pr_err("frame_vector_create() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	err = get_vaddr_frames(start & PAGE_MASK, nr, flags_vec, vec);
	if (err < 0) {
		pr_err("get_vaddr_frames() failed, err=%d\n", err);
		ret = err;
		goto err1;
	}

	nr_pages = frame_vector_count(vec);
	// pr_info("nr_pages=%d\n", nr_pages);

	err = frame_vector_to_pages(vec);
	if (err < 0) {
		pr_err("frame_vector_to_pages() failed, err=%d\n", err);
		ret = err;
		goto err2;
	}

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (! sgt) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	err = sg_alloc_table_from_pages(sgt, frame_vector_pages(vec), nr_pages, 0, length, GFP_KERNEL);
	if (err < 0) {
		pr_err("sg_alloc_table_from_pages() failed, err=%d\n", err);
		ret = err;
		goto err3;
	}

	err = dma_map_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (err < 0) {
		pr_err("dma_map_sgtable() failed, err=%d\n", err);
		ret = err;
		goto err4;
	}

	dma_sync_sgtable_for_device(self->dev, sgt, dma_dir);

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = -ENOMEM;
		goto err5;
	}

	err = self->buf_entry_from_sgt(self, sgt, buf, buf_entry);
	if (err < 0) {
		pr_err("self->buf_entry_from_sgt() failed, err=%d\n", err);
		ret = err;
		goto err6;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.userptr.sgt = sgt;

	qbuf_buf_entry(self, buf_entry);

	return 0;

err6:
	qvio_buf_entry_put(buf_entry);
err5:
	dma_unmap_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
err4:
	sg_free_table(sgt);
err3:
	kfree(sgt);
err2:
	put_vaddr_frames(vec);
err1:
	frame_vector_destroy(vec);
err0:
	return ret;
}

static long __file_ioctl_qbuf_dmabuf(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf) {
	long ret;
	int err;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	enum dma_data_direction dma_dir = buf->buf_dir;
	struct qvio_buf_entry* buf_entry;

	dmabuf = dma_buf_get(buf->u.fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get() failed, dmabuf=%p\n", dmabuf);

		ret = -EINVAL;
		goto err0;
	}
	// pr_info("dmabuf->size=%d\n", (int)dmabuf->size);

	attach = dma_buf_attach(dmabuf, self->dev);
	if (IS_ERR(attach)) {
		pr_err("dma_buf_attach() failed, attach=%p\n", attach);

		ret = -EINVAL;
		goto err1;
	}

	ret = dma_buf_begin_cpu_access(dmabuf, dma_dir);
	if (ret) {
		pr_err("dma_buf_begin_cpu_access() failed, ret=%ld\n", ret);
		goto err2;
	}

	sgt = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR(sgt)) {
		pr_err("dma_buf_map_attachment() failed, sgt=%p\n", sgt);

		ret = -EINVAL;
		goto err3;
	}

	// sgt_dump(sgt, true);

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = -ENOMEM;
		goto err4;
	}

	err = self->buf_entry_from_sgt(self, sgt, buf, buf_entry);
	if (err < 0) {
		pr_err("buf_entry_from_sgt() failed, err=%d\n", err);
		ret = err;
		goto err5;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.dmabuf.dmabuf = dmabuf;
	buf_entry->u.dmabuf.attach = attach;
	buf_entry->u.dmabuf.sgt = sgt;

	qbuf_buf_entry(self, buf_entry);

	return 0;

err5:
	qvio_buf_entry_put(buf_entry);
err4:
	dma_buf_unmap_attachment(attach, sgt, dma_dir);
err3:
	dma_buf_end_cpu_access(dmabuf, dma_dir);
err2:
	dma_buf_detach(dmabuf, attach);
err1:
	dma_buf_put(dmabuf);
err0:
	return ret;
}

static long __file_ioctl_qbuf_mmap(struct qvio_video_queue* self, struct file * filp, struct qvio_buffer* buf) {
	long ret;
	int err;
	ssize_t buf_size;
	void* vaddr;
	unsigned long nr_pages;
	struct page **pages;
	unsigned long i;
	struct sg_table* sgt;
	enum dma_data_direction dma_dir = buf->buf_dir;
	struct qvio_buf_entry* buf_entry;

	buf_size = utils_calc_buf_size(&self->format, buf->offset, buf->stride);
	if(buf_size <= 0) {
		pr_err("unexpected value, buf_size=%ld\n", buf_size);
		ret = buf_size;
		goto err0;
	}

	vaddr = (uint8_t*)self->mmap_buffer + buf->u.offset;
	// pr_info("vaddr=%llX\n", (int64_t)vaddr);

	nr_pages = (buf_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	// pr_info("nr_pages=%lu\n", nr_pages);

	pages = kmalloc(sizeof(struct page *) * nr_pages, GFP_KERNEL);
	if (!pages) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	for (i = 0; i < nr_pages; i++) {
		pages[i] = vmalloc_to_page(vaddr + (i << PAGE_SHIFT));
		if (!pages[i]) {
			pr_err("vmalloc_to_page() failed\n");
			ret = -ENOMEM;
			goto err1;
		}
	}

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt) {
		pr_err("kmalloc() failed\n");
		ret = -ENOMEM;
		goto err1;
	}

	err = sg_alloc_table_from_pages(sgt, pages, nr_pages, 0, buf_size, GFP_KERNEL);
	if (err < 0) {
		pr_err("sg_alloc_table_from_pages() failed, err=%d\n", err);
		ret = err;
		goto err2;
	}

	err = dma_map_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (err < 0) {
		pr_err("dma_map_sgtable() failed, err=%d\n", err);
		ret = err;
		goto err3;
	}

	// sgt_dump(sgt, true);

	buf_entry = qvio_buf_entry_new();
	if(! buf_entry) {
		ret = -ENOMEM;
		goto err4;
	}

	err = self->buf_entry_from_sgt(self, sgt, buf, buf_entry);
	if (err < 0) {
		pr_err("buf_entry_from_sgt() failed, err=%d\n", err);
		ret = err;
		goto err5;
	}

	buf_entry->buf = *buf;
	buf_entry->dma_dir = dma_dir;
	buf_entry->u.userptr.sgt = sgt;

	qbuf_buf_entry(self, buf_entry);

	kfree(pages);

	return 0;

err5:
	qvio_buf_entry_put(buf_entry);
err4:
	dma_unmap_sgtable(self->dev, sgt, dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
err3:
	sg_free_table(sgt);
err2:
	kfree(sgt);
err1:
	kfree(pages);
err0:
	return ret;
}

int qbuf_buf_entry(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry) {
	int err;
	struct qvio_buf_entry* next_entry;
	unsigned long flags;

	spin_lock_irqsave(&self->lock, flags);
	next_entry = list_empty(&self->job_list) ? buf_entry : NULL;
	list_add_tail(&buf_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->lock, flags);

#if 1
	if(self->state == QVIO_VIDEO_QUEUE_STATE_START && next_entry) {
		err = self->start_buf_entry(self, next_entry);
		if(err) {
			pr_err("self->start_buf_entry() failed, err=%d\n", err);
		}
	}
#endif

	return 0;
}

int qvio_video_queue_done(struct qvio_video_queue* self, struct qvio_buf_entry** next_entry) {
	int err;
	struct qvio_buf_entry* done_entry;

	spin_lock(&self->lock);
	if(list_empty(&self->job_list)) {
		spin_unlock(&self->lock);
		pr_err("self->job_list is empty\n");
		err = -ENXIO;
		goto err0;
	}

	// move job from job_list to done_list
	done_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
	list_del(&done_entry->node);
	// try pick next job
	*next_entry = list_empty(&self->job_list) ? NULL : list_first_entry(&self->job_list, struct qvio_buf_entry, node);

	list_add_tail(&done_entry->node, &self->done_list);
	spin_unlock(&self->lock);

	// job done wake up
	wake_up_interruptible(&self->irq_wait);

	return 0;

err0:
	return err;
}
