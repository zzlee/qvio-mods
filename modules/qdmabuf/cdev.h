#ifndef __QDMABUF_CDEV_H__
#define __QDMABUF_CDEV_H__

#include <linux/cdev.h>

struct qdmabuf_cdev {
	dev_t cdevno;
	struct cdev cdev;

	void* private_data;
	const struct file_operations* fops;
};

int qdmabuf_cdev_register(void);
void qdmabuf_cdev_unregister(void);

int qdmabuf_cdev_start(struct qdmabuf_cdev* self);
void qdmabuf_cdev_stop(struct qdmabuf_cdev* self);

// default file_operations
int qdmabuf_cdev_open(struct inode *inode, struct file *filp);
int qdmabuf_cdev_release(struct inode *inode, struct file *filep);

#endif // __QDMABUF_CDEV_H__