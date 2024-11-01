#ifndef __QVIO_CDEV_H__
#define __QVIO_CDEV_H__

#include <linux/cdev.h>

struct qvio_cdev {
	dev_t cdevno;
	struct cdev cdev;

	void* private_data;
	const struct file_operations* fops;
};

int qvio_cdev_register(void);
void qvio_cdev_unregister(void);

int qvio_cdev_start(struct qvio_cdev* self);
void qvio_cdev_stop(struct qvio_cdev* self);

// default file_operations
int qvio_cdev_open(struct inode *inode, struct file *filp);
int qvio_cdev_release(struct inode *inode, struct file *filep);

#endif // __QVIO_CDEV_H__