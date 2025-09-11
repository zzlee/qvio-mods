#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include "tpg.h"
#include "utils.h"
#include "xvidc.h"

static struct qvio_cdev_class __cdev_class;
static const unsigned int __reset_delay = 100;

static void __free(struct kref *ref);
static int __reset_cores(struct qvio_tpg* self);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static long __file_ioctl_s_fmt(struct qvio_tpg* self, struct file * filp, unsigned long arg);
static long __file_ioctl_g_fmt(struct qvio_tpg* self, struct file * filp, unsigned long arg);
static long __file_ioctl_streamon(struct qvio_tpg* self, struct file * filp, unsigned long arg);
static long __file_ioctl_streamoff(struct qvio_tpg* self, struct file * filp, unsigned long arg);
static long __file_ioctl_trigger(struct qvio_tpg* self, struct file * filp, unsigned long arg);

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
};

int qvio_tpg_register(void) {
	int err;

	pr_info("\n");

	err = qvio_cdev_register(&__cdev_class, 0, 255, "qtpg");
	if(err) {
		pr_err("qvio_cdev_register() failed\n");
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_tpg_unregister(void) {
	pr_info("\n");

	qvio_cdev_unregister(&__cdev_class);
}

struct qvio_tpg* qvio_tpg_new(void) {
	int err;
	struct qvio_tpg* self = kzalloc(sizeof(struct qvio_tpg), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	return self;

err0:
	return NULL;
}

struct qvio_tpg* qvio_tpg_get(struct qvio_tpg* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __free(struct kref *ref) {
	struct qvio_tpg* self = container_of(ref, struct qvio_tpg, ref);

	// pr_info("\n");

	kfree(self);
}

void qvio_tpg_put(struct qvio_tpg* self) {
	if (self)
		kref_put(&self->ref, __free);
}

int qvio_tpg_probe(struct qvio_tpg* self) {
	int err;
	XV_tpg* xtpg = &self->xtpg;
	int i;

	err = __reset_cores(self);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
		goto err0;
	}

	xtpg->Config = (XV_tpg_Config) {
		.DeviceId = 0,
		.BaseAddress = (UINTPTR)self->reg,
		.HasAxi4sSlave = 1,
		.PixPerClk = 4,
		.NumVidComponents = 3,
		.MaxWidth = 4096,
		.MaxHeight = 2160,
		.MaxDataWidth = 8,
		.SolidColorEnable = 1,
		.RampEnable = 0,
		.ColorBarEnable = 1,
		.DisplayPortEnable = 0,
		.ColorSweepEnable = 0,
		.ZoneplateEnable = 0,
		.ForegroundEnable = 0
	};
	xtpg->IsReady = XIL_COMPONENT_IS_READY;
	XV_tpg_DisableAutoRestart(xtpg);

	for(i = 0;i < 5;i++) {
		if(XV_tpg_IsIdle(xtpg))
			break;

		pr_info("wait for idle... %d\n", i);
		msleep(100);
	}

	if(! XV_tpg_IsIdle(xtpg)) {
		err = -EBUSY;
		pr_err("unexpected, XV_tpg_IsIdle()\n");
		goto err0;
	}

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev, &__cdev_class);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err1;
	}

	return 0;

err1:
err0:
	return err;
}

void qvio_tpg_remove(struct qvio_tpg* self) {
	XV_tpg* xtpg = &self->xtpg;

	qvio_cdev_stop(&self->cdev, &__cdev_class);
	XV_tpg_DisableAutoRestart(xtpg);
}

static int __reset_cores(struct qvio_tpg* self) {
	int err;
	struct qvio_zdev* zdev = self->zdev;

	pr_info("reset tpg... (0x%x)\n", self->reset_mask);
	err = qvio_zdev_reset_mask(zdev, self->reset_mask, __reset_delay);
	if(err < 0) {
		pr_err("qvio_zdev_reset_mask() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_tpg* self = filp->private_data;

	switch(cmd) {
	case QVIO_IOC_TPG_S_FMT:
		ret = __file_ioctl_s_fmt(self, filp, arg);
		break;

	case QVIO_IOC_TPG_G_FMT:
		ret = __file_ioctl_g_fmt(self, filp, arg);
		break;

	case QVIO_IOC_TPG_STREAMON:
		ret = __file_ioctl_streamon(self, filp, arg);
		break;

	case QVIO_IOC_TPG_STREAMOFF:
		ret = __file_ioctl_streamoff(self, filp, arg);
		break;

	case QVIO_IOC_TPG_TRIGGER:
		ret = __file_ioctl_trigger(self, filp, arg);
		break;

	default:
		pr_err("unexpected, cmd=%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long __file_ioctl_s_fmt(struct qvio_tpg* self, struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_format args;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		ret = -EFAULT;
		goto err0;
	}

	switch(args.fmt) {
	case FOURCC_RGB:
		self->nXtpgColorFormat = 0; // TPG RGB
		break;

	case FOURCC_Y444:
		self->nXtpgColorFormat = 1; // TPG YUV 444
		break;

	default:
		self->nXtpgColorFormat = -1;

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

static long __file_ioctl_g_fmt(struct qvio_tpg* self, struct file * filp, unsigned long arg) {
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

static long __file_ioctl_streamon(struct qvio_tpg* self, struct file * filp, unsigned long arg) {
	int err;
	long ret;
	struct qvio_tpg_config args;
	XV_tpg* xtpg = &self->xtpg;
	int i;

	ret = copy_from_user(&args, (void __user *)arg, sizeof(args));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	err = __reset_cores(self);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
		ret = err;
		goto err0;
	}

	XV_tpg_DisableAutoRestart(xtpg);
	for(i = 0;i < 5;i++) {
		if(XV_tpg_IsIdle(xtpg))
			break;

		pr_info("wait for idle... %d\n", i);
		msleep(100);
	}

	if(! XV_tpg_IsIdle(xtpg)) {
		ret = -EBUSY;
		pr_err("unexpected, XV_tpg_IsIdle()\n");
		goto err0;
	}

	self->cur_config = args;

	XV_tpg_Set_width(xtpg, self->format.width);
	XV_tpg_Set_height(xtpg, self->format.height);
	XV_tpg_Set_colorFormat(xtpg, self->nXtpgColorFormat);
	XV_tpg_Set_bckgndId(xtpg, XTPG_BKGND_COLOR_BARS);
	XV_tpg_Set_ovrlayId(xtpg, 0);

	if(args.bypass) {
		XV_tpg_Set_enableInput(xtpg, 1);
		XV_tpg_Set_passthruStartX(xtpg, 0);
		XV_tpg_Set_passthruStartY(xtpg, 0);
		XV_tpg_Set_passthruEndX(xtpg, 0);
		XV_tpg_Set_passthruEndY(xtpg, 0);

		XV_tpg_EnableAutoRestart(xtpg);
	} else {
		XV_tpg_Set_enableInput(xtpg, 0);
	}

	XV_tpg_Start(xtpg);

	return 0;

err0:
	return ret;
}

static long __file_ioctl_streamoff(struct qvio_tpg* self, struct file * filp, unsigned long arg) {
	long ret;
	XV_tpg* xtpg = &self->xtpg;
	int i;

	XV_tpg_DisableAutoRestart(xtpg);
	for(i = 0;i < 5;i++) {
		if(XV_tpg_IsIdle(xtpg))
			break;

		pr_info("wait for idle... %d\n", i);
		msleep(100);
	}

	if(! XV_tpg_IsIdle(xtpg)) {
		ret = -EBUSY;
		pr_err("unexpected, XV_tpg_IsIdle()\n");
		goto err0;
	}

	memset(&self->cur_config, 0, sizeof(struct qvio_tpg_config));

	return 0;

err0:
	return ret;
}

static long __file_ioctl_trigger(struct qvio_tpg* self, struct file * filp, unsigned long arg) {
	long ret;
	XV_tpg* xtpg = &self->xtpg;

	if(self->cur_config.bypass) {
		pr_err("unexpected value, self->cur_config.bypass=%d\n", self->cur_config.bypass);
		ret = -EINVAL;
		goto err0;
	}

	if(! XV_tpg_IsIdle(xtpg)) {
		ret = -EBUSY;
		// pr_err("unexpected, XV_tpg_IsIdle()\n");
		goto err0;
	}

	XV_tpg_Start(xtpg);

	return 0;

err0:
	return ret;
}
