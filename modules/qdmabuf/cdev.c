#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "cdev.h"

#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>

#define QDMABUF_NODE_NAME		"qdmabuf"
#define QDMABUF_MINOR_BASE		(0)
#define QDMABUF_MINOR_COUNT		(255)

static struct class *g_class = NULL;
static int g_major = 0;
static int g_cdevno_base = 0;

int qdmabuf_cdev_register(void) {
	int err;
	dev_t dev;

	// pr_info("\n");

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6,4,0)
	g_class = class_create(THIS_MODULE, QDMABUF_NODE_NAME);
#else
	g_class = class_create(QDMABUF_NODE_NAME);
#endif
	if (IS_ERR(g_class)) {
		pr_err("class_create() failed, g_class=%p\n", g_class);
		err = -EINVAL;
		goto err0;
	}

	err = alloc_chrdev_region(&dev, QDMABUF_MINOR_BASE, QDMABUF_MINOR_COUNT, QDMABUF_NODE_NAME);
	if (err) {
		pr_err("alloc_chrdev_region() failed, err=%d\n", err);
		goto err1;
	}

	g_major = MAJOR(dev);
	// pr_info("g_major=%d\n", g_major);

	g_cdevno_base = 0;

	return err;

err1:
	class_destroy(g_class);
err0:
	return err;
}

void qdmabuf_cdev_unregister(void) {
	// pr_info("\n");

	unregister_chrdev_region(MKDEV(g_major, QDMABUF_MINOR_BASE), QDMABUF_MINOR_COUNT);
	class_destroy(g_class);
}

int qdmabuf_cdev_start(struct qdmabuf_cdev* self) {
	int err;
	struct device* new_device;

	// pr_info("\n");

	self->cdevno = MKDEV(g_major, g_cdevno_base++);
	cdev_init(&self->cdev, self->fops);
	self->cdev.owner = THIS_MODULE;
	err = cdev_add(&self->cdev, self->cdevno, 1);
	if (err) {
		pr_err("cdev_add() failed, err=%d\n", err);
		goto err0;
	}

	new_device = device_create(g_class, NULL, self->cdevno, self,
		QDMABUF_NODE_NAME "%d", MINOR(self->cdevno));
	if (IS_ERR(new_device)) {
		pr_err("device_create() failed, new_device=%p\n", new_device);
		goto err1;
	}

	return 0;

err1:
	cdev_del(&self->cdev);
err0:
	return err;
}

void qdmabuf_cdev_stop(struct qdmabuf_cdev* self) {
	// pr_info("\n");

	device_destroy(g_class, self->cdevno);
	cdev_del(&self->cdev);
}

int qdmabuf_cdev_open(struct inode *inode, struct file *filp) {
	struct qdmabuf_cdev* self = container_of(inode->i_cdev, struct qdmabuf_cdev, cdev);

	// pr_info("self=%p\n", self);

	filp->private_data = self->private_data;
	nonseekable_open(inode, filp);

	return 0;
}

int qdmabuf_cdev_release(struct inode *inode, struct file *filep) {
	// struct qdmabuf_cdev* self = container_of(inode->i_cdev, struct qdmabuf_cdev, cdev);

	// pr_info("self=%p\n", self);

	return 0;
}
