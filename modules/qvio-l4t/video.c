#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "video.h"

#include <linux/kernel.h>
#include <linux/version.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>

static int __ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *capability);
static int __ioctl_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *format);
static int __ioctl_g_fmt(struct file *file, void *fh, struct v4l2_format *format);
static int __ioctl_s_fmt(struct file *file, void *fh, struct v4l2_format *format);
static int __ioctl_try_fmt(struct file *file, void *fh, struct v4l2_format *format);
static int __ioctl_enum_input(struct file *file, void *fh, struct v4l2_input *input);
static int __ioctl_g_input(struct file *file, void *fh, unsigned int *input);
static int __ioctl_s_input(struct file *file, void *fh, unsigned int input);
static int __ioctl_enum_output(struct file *file, void *fh, struct v4l2_output *output);
static int __ioctl_g_output(struct file *file, void *fh, unsigned int *output);
static int __ioctl_s_output(struct file *file, void *fh, unsigned int output);
static int __ioctl_g_parm(struct file *file, void *fh, struct v4l2_streamparm *param);
static int __ioctl_s_parm(struct file *file, void *fh, struct v4l2_streamparm *param);
static int __ioctl_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *frame_sizes);
static int __ioctl_enum_frameintervals(struct file *file, void *fh, struct v4l2_frmivalenum *frame_intervals);
static long __ioctl_default(struct file *file, void *fh, bool valid_prio, unsigned int cmd, void *arg);

static const struct v4l2_file_operations __video_fops = {
	.owner          = THIS_MODULE    ,
	.open           = v4l2_fh_open   ,
	.release        = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2   ,
	.read           = vb2_fop_read   ,
	.write          = vb2_fop_write  ,
	.mmap           = vb2_fop_mmap   ,
	.poll           = vb2_fop_poll   ,
};

static const struct v4l2_ioctl_ops __v4l2_ioctl_ops = {
	.vidioc_querycap               = __ioctl_querycap,
	.vidioc_enum_fmt_vid_cap       = __ioctl_enum_fmt,
	.vidioc_enum_fmt_vid_out       = __ioctl_enum_fmt,
	.vidioc_g_fmt_vid_cap          = __ioctl_g_fmt,
	.vidioc_g_fmt_vid_out          = __ioctl_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane   = __ioctl_g_fmt,
	.vidioc_g_fmt_vid_out_mplane   = __ioctl_g_fmt,
	.vidioc_s_fmt_vid_cap          = __ioctl_s_fmt,
	.vidioc_s_fmt_vid_out          = __ioctl_s_fmt,
	.vidioc_s_fmt_vid_cap_mplane   = __ioctl_s_fmt,
	.vidioc_s_fmt_vid_out_mplane   = __ioctl_s_fmt,
	.vidioc_try_fmt_vid_cap        = __ioctl_try_fmt,
	.vidioc_try_fmt_vid_out        = __ioctl_try_fmt,
	.vidioc_try_fmt_vid_cap_mplane = __ioctl_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = __ioctl_try_fmt,
	.vidioc_reqbufs                = vb2_ioctl_reqbufs,
	.vidioc_querybuf               = vb2_ioctl_querybuf,
	.vidioc_qbuf                   = vb2_ioctl_qbuf,
	.vidioc_expbuf                 = vb2_ioctl_expbuf,
	.vidioc_dqbuf                  = vb2_ioctl_dqbuf,
	.vidioc_create_bufs            = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf            = vb2_ioctl_prepare_buf,
	.vidioc_streamon               = vb2_ioctl_streamon,
	.vidioc_streamoff              = vb2_ioctl_streamoff,
	.vidioc_enum_input             = __ioctl_enum_input,
	.vidioc_g_input                = __ioctl_g_input,
	.vidioc_s_input                = __ioctl_s_input,
	.vidioc_enum_output            = __ioctl_enum_output,
	.vidioc_g_output               = __ioctl_g_output,
	.vidioc_s_output               = __ioctl_s_output,
	.vidioc_g_parm                 = __ioctl_g_parm,
	.vidioc_s_parm                 = __ioctl_s_parm,
	.vidioc_log_status             = v4l2_ctrl_log_status,
	.vidioc_enum_framesizes        = __ioctl_enum_framesizes,
	.vidioc_enum_frameintervals    = __ioctl_enum_frameintervals,
	.vidioc_subscribe_event        = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event      = v4l2_event_unsubscribe,
	.vidioc_default                = __ioctl_default,
};

struct qvio_video* qvio_video_new(void) {
	struct qvio_video* self = kzalloc(sizeof(struct qvio_video), GFP_KERNEL);

	kref_init(&self->ref);
	qvio_queue_init(&self->queue);
	mutex_init(&self->device_mutex);
	self->vfl_dir = VFL_DIR_RX;
	self->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	self->buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	self->halign = 0x40;
	self->valign = 1;

	return self;
}

struct qvio_video* qvio_video_get(struct qvio_video* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __video_free(struct kref *ref)
{
	struct qvio_video* self = container_of(ref, struct qvio_video, ref);

	pr_info("\n");

	kfree(self);
}

void qvio_video_put(struct qvio_video* self) {
	if (self)
		kref_put(&self->ref, __video_free);
}

int qvio_video_start(struct qvio_video* self) {
	int err;
	struct vb2_queue* vb2_queue;

	pr_info("self=%p\n", self);

	err = v4l2_device_register(NULL, &self->v4l2_dev);
	if(err) {
		pr_err("v4l2_device_register() failed, err=%d\n", err);
		goto err0;
	}

	self->queue.halign = self->halign;
	self->queue.valign = self->valign;

	err = qvio_queue_start(&self->queue, self->buffer_type);
	if(err) {
		pr_err("qvio_queue_start() failed, err=%d\n", err);
		goto err1;
	}

	vb2_queue = qvio_queue_get_vb2_queue(&self->queue);
	err = vb2_queue_init(vb2_queue);
	if(err) {
		pr_err("vb2_queue_init() failed, err=%d\n", err);
		goto err2;
	}

	self->vdev = video_device_alloc();
	if(! self->vdev) {
		pr_err("video_device_alloc() failed\n");
		goto err2;
	}

	pr_info("param: %s %d\n", self->v4l2_dev.name, self->vfl_dir);

	self->current_format.type = self->buffer_type;

	switch(self->buffer_type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		self->current_format.fmt.pix.width = 1920;
		self->current_format.fmt.pix.height = 1080;
		self->current_format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
		self->current_format.fmt.pix.bytesperline =
			ALIGN(self->current_format.fmt.pix.width, self->halign);
		self->current_format.fmt.pix.sizeimage =
			self->current_format.fmt.pix.bytesperline * ALIGN(self->current_format.fmt.pix.height, self->valign) * 3 / 2;

#if 0 // DEBUG
		pr_info("bytesperline=%d sizeimage=%d\n",
			(int)self->current_format.fmt.pix.bytesperline,
			(int)self->current_format.fmt.pix.sizeimage);
#endif
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		self->current_format.fmt.pix_mp.width = 1920;
		self->current_format.fmt.pix_mp.height = 1080;
		self->current_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
		self->current_format.fmt.pix_mp.num_planes = 2;
		self->current_format.fmt.pix_mp.plane_fmt[0].bytesperline =
			ALIGN(self->current_format.fmt.pix_mp.width, self->halign);
		self->current_format.fmt.pix_mp.plane_fmt[0].sizeimage =
			self->current_format.fmt.pix_mp.plane_fmt[0].bytesperline * ALIGN(self->current_format.fmt.pix_mp.height, self->valign);
		self->current_format.fmt.pix_mp.plane_fmt[1].bytesperline =
			ALIGN(self->current_format.fmt.pix_mp.width, self->halign);
		self->current_format.fmt.pix_mp.plane_fmt[1].sizeimage =
			self->current_format.fmt.pix_mp.plane_fmt[1].bytesperline * ALIGN(self->current_format.fmt.pix_mp.height, self->valign) / 2;

#if 0 // DEBUG
		pr_info("bytesperline=%d/%d sizeimage=%d/%d\n",
			(int)self->current_format.fmt.pix_mp.plane_fmt[0].bytesperline,
			(int)self->current_format.fmt.pix_mp.plane_fmt[1].bytesperline,
			(int)self->current_format.fmt.pix_mp.plane_fmt[0].sizeimage,
			(int)self->current_format.fmt.pix_mp.plane_fmt[1].sizeimage);
#endif
		break;

	default:
		pr_err("unexpected value, self->buffer_type=%d\n", (int)self->buffer_type);
		goto err3;
		break;
	}

	self->current_parm.type = self->buffer_type;
	self->current_parm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	self->current_parm.parm.capture.capturemode = 0;
	self->current_parm.parm.capture.timeperframe.numerator = 60;
	self->current_parm.parm.capture.timeperframe.denominator = 1;

	snprintf(self->vdev->name, sizeof(self->vdev->name), "%s", self->v4l2_dev.name);
	self->vdev->v4l2_dev = &self->v4l2_dev;
	self->vdev->vfl_dir = self->vfl_dir;
	self->vdev->fops = &__video_fops;
	self->vdev->ioctl_ops = &__v4l2_ioctl_ops;
	self->vdev->tvnorms = V4L2_STD_ALL;
	self->vdev->release = video_device_release_empty;
	self->vdev->queue = vb2_queue;
	self->vdev->lock = &self->device_mutex;
	video_set_drvdata(self->vdev, self);
	self->vdev->device_caps = self->device_caps;
	err = video_register_device(self->vdev, VFL_TYPE_VIDEO, -1);
	if(err) {
		pr_err("video_register_device() failed, err=%d\n", err);
		goto err3;
	}

	return 0;

err3:
	video_device_release(self->vdev);
err2:
	qvio_queue_stop(&self->queue);
err1:
	v4l2_device_unregister(&self->v4l2_dev);
err0:
	return err;
}

void qvio_video_stop(struct qvio_video* self) {
	pr_info("\n");

	video_unregister_device(self->vdev);
	video_device_release(self->vdev);
	qvio_queue_stop(&self->queue);
	v4l2_device_unregister(&self->v4l2_dev);
}

int qvio_video_try_fmt(struct qvio_video* self, struct v4l2_format *format) {
	int err;

	pr_info("\n");

	if(format->type != self->buffer_type) {
		pr_err("unexpected value, %d != %d\n", (int)format->type, (int)self->buffer_type);
		err = -EINVAL;

		goto err0;
	}

	switch(self->buffer_type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		switch(format->fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_M420:
			break;

		default:
			pr_err("unexpected value, format->fmt.pix.pixelformat=0x%X\n", (int)format->fmt.pix.pixelformat);
			err = -EINVAL;

			goto err0;
			break;
		}
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		switch(format->fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_M420:
			break;

		default:
			pr_err("unexpected value, format->fmt.pix_mp.pixelformat=0x%X\n", (int)format->fmt.pix_mp.pixelformat);
			err = -EINVAL;

			goto err0;
			break;
		}
		break;

	default:
		pr_err("unexpected value, self->buffer_type=%d\n", (int)self->buffer_type);
		err = -EINVAL;

		goto err0;
		break;
	}

	err = 0;

	return err;

err0:
	return err;
}

long qvio_video_buf_done(struct qvio_video* self) {
	long ret;
	int err;

#if 0 // DEBUG
	pr_info("\n");
#endif

	err = qvio_queue_try_buf_done(&self->queue);
	if(err) {
		pr_err("qvio_queue_try_buf_done() failed, err=%d", err);
		ret = err;

		goto err0;
	}

	ret = 0;

	return ret;

err0:
	return ret;
}

static int __ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *capability) {
	struct qvio_video* self = video_drvdata(file);

	pr_info("self=%p\n", self);

	memset(capability, 0, sizeof(struct v4l2_capability));
	capability->version = LINUX_VERSION_CODE;
	snprintf(capability->driver, sizeof(capability->driver), "qvio");
	snprintf(capability->card, sizeof(capability->card), "qvio");
	strlcpy(capability->bus_info, self->bus_info, sizeof(capability->bus_info));
	capability->capabilities = self->device_caps | V4L2_CAP_DEVICE_CAPS;
	capability->device_caps = self->device_caps;

	return 0;
}

static int __ioctl_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *format) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(format->type != self->buffer_type) {
		pr_err("unexpected value, %d != %d\n", (int)format->type, (int)self->buffer_type);
		err = -EINVAL;

		goto err0;
	}

	switch(format->index) {
	case 0:
		format->flags = 0;
		snprintf((char *) format->description, sizeof(format->description), "YUYV");
		format->pixelformat = V4L2_PIX_FMT_YUYV;
		break;

	case 1:
		format->flags = 0;
		snprintf((char *) format->description, sizeof(format->description), "NV12");
		format->pixelformat = V4L2_PIX_FMT_NV12;
		break;

	case 2:
		format->flags = 0;
		snprintf((char *) format->description, sizeof(format->description), "M420");
		format->pixelformat = V4L2_PIX_FMT_M420;
		break;

	default:
		pr_err("unexpected value, format->index=%d\n", (int)format->index);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_g_fmt(struct file *file, void *fh, struct v4l2_format *format) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	memcpy(format, &self->current_format, sizeof(struct v4l2_format));

	return 0;
}

static int __ioctl_s_fmt(struct file *file, void *fh, struct v4l2_format *format) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	err = qvio_video_try_fmt(self, format);
	if(err) {
		pr_err("qvio_video_try_fmt() failed, err=%d", err);
		goto err0;
	}

	switch(format->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		switch(format->fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			format->fmt.pix.bytesperline = ALIGN(format->fmt.pix.width * 2, self->halign);
			format->fmt.pix.sizeimage = format->fmt.pix.bytesperline * ALIGN(format->fmt.pix.height, self->valign);
			break;

		case V4L2_PIX_FMT_NV12:
			format->fmt.pix.bytesperline = ALIGN(format->fmt.pix.width, self->halign);
			format->fmt.pix.sizeimage = format->fmt.pix.bytesperline * ALIGN(format->fmt.pix.height, self->valign) * 3 / 2;
			break;

		case V4L2_PIX_FMT_M420:
			format->fmt.pix.bytesperline = ALIGN(format->fmt.pix.width, self->halign);
			format->fmt.pix.sizeimage = format->fmt.pix.bytesperline * ALIGN(format->fmt.pix.height * 3 / 2, self->valign);
			break;

		default:
			pr_err("invalid value, format->fmt.pix.pixelformat=%d", (int)format->fmt.pix.pixelformat);
			err = -EINVAL;
			goto err0;
			break;
		}

#if 0 // DEBUG
		pr_info("%dx%d bytesperline=%d sizeimage=%d\n",
			format->fmt.pix.width, format->fmt.pix.height,
			(int)format->fmt.pix.bytesperline,
			(int)format->fmt.pix.sizeimage);
#endif

		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		switch(format->fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			format->fmt.pix_mp.num_planes = 1;
			format->fmt.pix_mp.plane_fmt[0].bytesperline = ALIGN(format->fmt.pix_mp.width * 2, self->halign);
			format->fmt.pix_mp.plane_fmt[0].sizeimage =
				format->fmt.pix_mp.plane_fmt[0].bytesperline * ALIGN(format->fmt.pix_mp.height, self->valign);
			break;

		case V4L2_PIX_FMT_NV12:
			format->fmt.pix_mp.num_planes = 2;
			format->fmt.pix_mp.plane_fmt[0].bytesperline = ALIGN(format->fmt.pix_mp.width, self->halign);
			format->fmt.pix_mp.plane_fmt[0].sizeimage =
				format->fmt.pix_mp.plane_fmt[0].bytesperline * ALIGN(format->fmt.pix_mp.height, self->valign);
			format->fmt.pix_mp.plane_fmt[1].bytesperline = format->fmt.pix_mp.plane_fmt[0].bytesperline;
			format->fmt.pix_mp.plane_fmt[1].sizeimage =
				format->fmt.pix_mp.plane_fmt[1].bytesperline * ALIGN(format->fmt.pix_mp.height, self->valign) / 2;
			break;

		case V4L2_PIX_FMT_M420:
			format->fmt.pix_mp.num_planes = 1;
			format->fmt.pix_mp.plane_fmt[0].bytesperline = ALIGN(format->fmt.pix_mp.width, self->halign);
			format->fmt.pix_mp.plane_fmt[0].sizeimage =
				format->fmt.pix_mp.plane_fmt[0].bytesperline * ALIGN(format->fmt.pix_mp.height * 3 / 2, self->valign);
			break;

		default:
			pr_err("invalid value, format->fmt.pix_mp.pixelformat=%d", (int)format->fmt.pix_mp.pixelformat);
			err = -EINVAL;
			goto err0;
			break;
		}

#if 0 // DEBUG
		pr_info("%dx%d bytesperline={%d,%d,%d,%d} sizeimage={%d,%d,%d,%d}\n",
			format->fmt.pix_mp.width, format->fmt.pix_mp.height,
			(int)format->fmt.pix_mp.plane_fmt[0].bytesperline,
			(int)format->fmt.pix_mp.plane_fmt[1].bytesperline,
			(int)format->fmt.pix_mp.plane_fmt[2].bytesperline,
			(int)format->fmt.pix_mp.plane_fmt[3].bytesperline,
			(int)format->fmt.pix_mp.plane_fmt[0].sizeimage,
			(int)format->fmt.pix_mp.plane_fmt[1].sizeimage,
			(int)format->fmt.pix_mp.plane_fmt[2].sizeimage,
			(int)format->fmt.pix_mp.plane_fmt[3].sizeimage);
#endif

		break;

	default:
		pr_err("unexpected value, format->type=%d\n", (int)format->type);
		err = -EINVAL;
		goto err0;
		break;
	}

	memcpy(&self->current_format, format, sizeof(struct v4l2_format));

	err = qvio_queue_s_fmt(&self->queue, format);
	if(err) {
		pr_err("qvio_queue_s_fmt() failed, err=%d", err);
		goto err0;
	}

#if 0 // TODO
	err = qvio_user_job_s_fmt(&self->user_job_ctrl, format);
	if(err) {
		pr_warn("qvio_user_job_s_fmt() failed, err=%d", err);
	}
#endif

	return 0;

err0:
	return err;
}

static int __ioctl_try_fmt(struct file *file, void *fh, struct v4l2_format *format) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	err = qvio_video_try_fmt(self, format);
	if(err) {
		pr_err("qvio_video_try_fmt() failed, err=%d", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_enum_input(struct file *file, void *fh, struct v4l2_input *input) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_RX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	switch(input->index) {
	case 0:
		snprintf((char *) input->name, sizeof(input->name), "qvio-input");
		input->type = V4L2_INPUT_TYPE_CAMERA;
		input->audioset = 0;
		input->tuner = 0;
		input->std = 0;
		input->status = 0;
		input->capabilities = 0;
		break;

	default:
		pr_err("unexpected value, input->index=%d\n", (int)input->index);
		err = -EINVAL;

		goto err0;
		break;
	}

	return err;

err0:
	return err;
}

static int __ioctl_g_input(struct file *file, void *fh, unsigned int *input) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_RX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	*input = self->current_inout;

	return 0;

err0:
	return err;
}

static int __ioctl_s_input(struct file *file, void *fh, unsigned int input) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_RX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	switch(input) {
	case 0:
		self->current_inout = input;
		break;

	default:
		pr_err("unexpected value, input=%d\n", (int)input);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_enum_output(struct file *file, void *fh, struct v4l2_output *output) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_TX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	switch(output->index) {
	case 0:
		snprintf((char *) output->name, sizeof(output->name), "qvio-output");
		output->type = V4L2_OUTPUT_TYPE_ANALOG;
		output->audioset = 0;
		output->modulator = 0;
		output->std = 0;
		output->capabilities = 0;
		break;

	default:
		pr_err("unexpected value, output->index=%d\n", (int)output->index);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_g_output(struct file *file, void *fh, unsigned int *output) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_TX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	*output = self->current_inout;

	return 0;

err0:
	return err;
}

static int __ioctl_s_output(struct file *file, void *fh, unsigned int output) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(self->vfl_dir != VFL_DIR_TX) {
		pr_err("unexpected value, self->vfl_dir=%d\n", (int)self->vfl_dir);
		err = -EINVAL;

		goto err0;
	}

	switch(output) {
	case 0:
		self->current_inout = output;
		break;

	default:
		pr_err("unexpected value, output=%d\n", (int)output);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_g_parm(struct file *file, void *fh, struct v4l2_streamparm *param) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(param->type != self->buffer_type) {
		pr_err("unexpected value, %d != %d\n", (int)param->type, (int)self->buffer_type);
		err = -EINVAL;

		goto err0;
	}

	*param = self->current_parm;

	return 0;

err0:
	return err;
}

static int __ioctl_s_parm(struct file *file, void *fh, struct v4l2_streamparm *param) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	if(param->type != self->buffer_type) {
		pr_err("unexpected value, %d != %d\n", (int)param->type, (int)self->buffer_type);
		err = -EINVAL;

		goto err0;
	}

	self->current_parm = *param;

	return 0;

err0:
	return err;
}

static int __ioctl_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *frame_sizes) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	switch(frame_sizes->index) {
	case 0:
		switch(frame_sizes->pixel_format) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_M420:
			frame_sizes->type = V4L2_FRMSIZE_TYPE_STEPWISE;
			frame_sizes->stepwise.min_width = 64;
			frame_sizes->stepwise.max_width = 4096;
			frame_sizes->stepwise.step_width = 8;
			frame_sizes->stepwise.min_height = 64;
			frame_sizes->stepwise.max_height = 4096;
			frame_sizes->stepwise.step_height = 8;
			break;

		default:
			pr_err("unexpected value, frame_sizes->pixel_format=0x%X\n", (int)frame_sizes->pixel_format);
			err = -EINVAL;

			goto err0;
			break;
		}
		break;

	default:
		pr_err("unexpected value, frame_sizes->index=%d\n", (int)frame_sizes->index);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static int __ioctl_enum_frameintervals(struct file *file, void *fh, struct v4l2_frmivalenum *frame_intervals) {
	int err;
	struct qvio_video* self = video_drvdata(file);

	pr_info("\n");

	switch(frame_intervals->index) {
	case 0:
		switch(frame_intervals->pixel_format) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_M420:
			frame_intervals->type = V4L2_FRMIVAL_TYPE_DISCRETE;
			frame_intervals->discrete.numerator = 1;
			frame_intervals->discrete.denominator = 60;
			break;

		default:
			pr_err("unexpected value, frame_intervals->pixel_format=0x%X\n", (int)frame_intervals->pixel_format);
			err = -EINVAL;

			goto err0;
			break;
		}
		break;

	default:
		pr_err("unexpected value, frame_intervals->index=%d\n", (int)frame_intervals->index);
		err = -EINVAL;

		goto err0;
		break;
	}

	return 0;

err0:
	return err;
}

static long __ioctl_default(struct file *file, void *fh, bool valid_prio, unsigned int cmd, void *arg) {
	long ret;
	struct qvio_video* self = video_drvdata(file);

#if 0
	pr_info("valid_prio=%d cmd=%d arg=%p\n", valid_prio, cmd, arg);
#endif

	switch(cmd) {
#if 0
	case QVID_IOC_USER_JOB_FD: {
		int* pFd = (int*)arg;

		*pFd = __anon_fd("qvio-user-job", self->user_job_ctrl.ctrl_fops, &self->user_job_ctrl, O_RDONLY | O_CLOEXEC);
		ret = 0;

		pr_info("fd=%d\n", *pFd);
	}
		break;

	case QVID_IOC_BUF_DONE:
		ret = qvio_video_buf_done(self);
		break;
#endif

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
