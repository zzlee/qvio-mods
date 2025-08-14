#ifndef __QVIO_CDEV_H__
#define __QVIO_CDEV_H__

#include <linux/cdev.h>

struct qvio_cdev_class {
	int base;
	int count;
	const char * name;

	struct class* cls;
	int major;
	int next_minor;
};

struct qvio_cdev {
	dev_t cdevno;
	struct cdev cdev;

	void* private_data;
	const struct file_operations* fops;
};

int qvio_cdev_register(struct qvio_cdev_class* self, unsigned base, unsigned count, const char * name);
void qvio_cdev_unregister(struct qvio_cdev_class* self);

int qvio_cdev_start(struct qvio_cdev* self, struct qvio_cdev_class* cls);
void qvio_cdev_stop(struct qvio_cdev* self, struct qvio_cdev_class* cls);

// default file_operations
int qvio_cdev_open(struct inode *inode, struct file *filp);
int qvio_cdev_release(struct inode *inode, struct file *filep);

#endif // __QVIO_CDEV_H__