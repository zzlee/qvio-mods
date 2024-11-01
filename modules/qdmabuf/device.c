#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "device.h"
#include "cdev.h"
#include "ioctl.h"
#include "uapi/qdmabuf.h"

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/compat.h>

#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif

static int __probe(struct platform_device *pdev);
static int __remove(struct platform_device *pdev);

static struct qdmabuf_device* __device_new(void);
static struct qdmabuf_device* __device_get(struct qdmabuf_device* self);
static void __device_put(struct qdmabuf_device* self);

static int __device_start(struct qdmabuf_device* self);
static void __device_stop(struct qdmabuf_device* self);

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static long __file_ioctl_compat(struct file *filep, unsigned int cmd, unsigned long arg);
#endif

#ifdef CONFIG_OF
extern struct of_device_id qdmabuf_of_match[];
#endif // CONFIG_OF

static struct platform_driver __driver = {
	.driver = {
		.name = QDMABUF_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= qdmabuf_of_match,
#endif // CONFIG_OF
	},
	.probe  = __probe,
	.remove = __remove,
};

static struct file_operations __fops = {
	.open = qdmabuf_cdev_open,
	.release = qdmabuf_cdev_release,
	.unlocked_ioctl = __file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = __file_ioctl_compat,
#endif
};

static int __probe(struct platform_device *pdev) {
	int err = 0;
	struct qdmabuf_device* self;

	self = __device_new();
	if(! self) {
		pr_err("__device_new() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	// pr_info("self=%p\n", self);

	self->pdev = pdev;
	platform_set_drvdata(pdev, self);

	err = __device_start(self);
	if(err) {
		pr_err("__device_start() failed, err=%d\n", err);
		goto err1;
	}

	return err;

err1:
	__device_put(self);
err0:
	return err;
}

static int __remove(struct platform_device *pdev) {
	int err = 0;
	struct qdmabuf_device* self = platform_get_drvdata(pdev);

	// pr_info("self=%p\n", self);

	__device_stop(self);
	__device_put(self);
	platform_set_drvdata(pdev, NULL);

	return err;
}

int qdmabuf_device_register(void) {
	int err;

	// pr_info("\n");

	err = qdmabuf_cdev_register();
	if (err != 0) {
		pr_err("qdmabuf_cdev_register() failed, err=%d\n", err);
		goto err0;
	}

	err = platform_driver_register(&__driver);
	if(err) {
		pr_err("platform_driver_register() failed\n");
		goto err1;
	}

	return err;

err1:
	qdmabuf_cdev_unregister();
err0:
	return err;
}

void qdmabuf_device_unregister(void) {
	// pr_info("\n");

	platform_driver_unregister(&__driver);
	qdmabuf_cdev_unregister();
}

static struct qdmabuf_device* __device_new(void) {
	struct qdmabuf_device* self = kzalloc(sizeof(struct qdmabuf_device), GFP_KERNEL);

	kref_init(&self->ref);

	return self;
}

static struct qdmabuf_device* __device_get(struct qdmabuf_device* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __device_free(struct kref *ref)
{
	struct qdmabuf_device* self = container_of(ref, struct qdmabuf_device, ref);

	// pr_info("\n");

	kfree(self);
}

static void __device_put(struct qdmabuf_device* self) {
	if (self)
		kref_put(&self->ref, __device_free);
}

static int __device_start(struct qdmabuf_device* self) {
	int err;

	// pr_info("\n");

	self->cdev.private_data = self;
	self->cdev.fops = &__fops;

	err = qdmabuf_cdev_start(&self->cdev);
	if(err) {
		pr_err("qdmabuf_cdev_start() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

static void __device_stop(struct qdmabuf_device* self) {
	// pr_info("\n");

	qdmabuf_cdev_stop(&self->cdev);
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qdmabuf_device* device = filp->private_data;

	switch(cmd) {
	case QDMABUF_IOCTL_ALLOC:
		ret = qdmabuf_ioctl_alloc(device, arg);
		break;

	case QDMABUF_IOCTL_INFO:
		ret = qdmabuf_ioctl_info(device, arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long __file_ioctl_compat(struct file *filep, unsigned int cmd, unsigned long arg) {
	return __file_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif
