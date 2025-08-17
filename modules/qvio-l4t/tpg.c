#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "tpg.h"
#include "uapi/qvio-l4t.h"
#include "utils.h"

static struct qvio_cdev_class __cdev_class;
static const int __reset_mask = 0x03; // [tpg, x, x]
static const unsigned int __reset_delay = 100;

static void __free(struct kref *ref);
static int __reset_cores(struct qvio_tpg* self);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);

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

	err = __reset_cores(self);
	if(err < 0) {
		pr_err("__reset_cores() failed, err=%d\n", err);
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
	qvio_cdev_stop(&self->cdev, &__cdev_class);
}

static int __reset_cores(struct qvio_tpg* self) {
	int err;
	struct qvio_zdev* zdev = self->zdev;

	pr_info("reset tpg...\n");
	err = qvio_zdev_reset_mask(zdev, __reset_mask, __reset_delay);
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
	// TODO:

	default:
		pr_err("unexpected, cmd=%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
