#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "cdev.h"

#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>

int qvio_cdev_register(struct qvio_cdev_class* self, unsigned base, unsigned count, const char * name) {
	int err;
	dev_t dev;

	// pr_info("%d, %d, %s\n", base, count, name);

	self->base = base;
	self->count = count;
	self->name = name;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6,4,0)
	self->cls = class_create(THIS_MODULE, name);
#else
	self->cls = class_create(name);
#endif
	if (IS_ERR(self->cls)) {
		pr_err("class_create() failed, self->cls=%p\n", self->cls);
		err = -EINVAL;
		goto err0;
	}

	err = alloc_chrdev_region(&dev, base, count, name);
	if (err) {
		pr_err("alloc_chrdev_region() failed, err=%d\n", err);
		goto err1;
	}

	self->major = MAJOR(dev);
	// pr_info("self->major=%d\n", MAJOR(dev));

	self->next_minor = base;

	return err;

err1:
	class_destroy(self->cls);
err0:
	return err;
}

void qvio_cdev_unregister(struct qvio_cdev_class* self) {
	// pr_info("\n");

	unregister_chrdev_region(MKDEV(self->major, self->base), self->count);
	class_destroy(self->cls);
}

int qvio_cdev_start(struct qvio_cdev* self, struct qvio_cdev_class* cls) {
	int err;
	struct device* new_device;
	char name[256];

	// pr_info("self=%p, cls=%p\n", self, cls);

	self->cdevno = MKDEV(cls->major, cls->next_minor);
	cdev_init(&self->cdev, self->fops);
	self->cdev.owner = THIS_MODULE;
	err = cdev_add(&self->cdev, self->cdevno, 1);
	if (err) {
		pr_err("cdev_add() failed, err=%d\n", err);
		goto err0;
	}

	sprintf(name, "%s%%d", cls->name);

	new_device = device_create(cls->cls, NULL, self->cdevno, self, name, MINOR(self->cdevno));
	if (IS_ERR(new_device)) {
		pr_err("device_create() failed, new_device=%p\n", new_device);
		goto err1;
	}

	cls->next_minor++;

	return 0;

err1:
	cdev_del(&self->cdev);
err0:
	return err;
}

void qvio_cdev_stop(struct qvio_cdev* self, struct qvio_cdev_class* cls) {
	// pr_info("self=%p, cls=%p\n", self, cls);

	device_destroy(cls->cls, self->cdevno);
	cdev_del(&self->cdev);
}

int qvio_cdev_open(struct inode *inode, struct file *filp) {
	struct qvio_cdev* self = container_of(inode->i_cdev, struct qvio_cdev, cdev);

	// pr_info("self=%p\n", self);

	filp->private_data = self->private_data;
	nonseekable_open(inode, filp);

	return 0;
}

int qvio_cdev_release(struct inode *inode, struct file *filep) {
	// struct qvio_cdev* self = container_of(inode->i_cdev, struct qvio_cdev, cdev);

	// pr_info("self=%p\n", self);

	return 0;
}
