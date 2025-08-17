#ifndef __QVIO_ZDEV_H__
#define __QVIO_ZDEV_H__

#include <linux/platform_device.h>
#include <linux/mutex.h>

#include "cdev.h"

struct qvio_zdev {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;
	struct qvio_cdev cdev;

	void __iomem * reg;
	struct mutex mutex_reg;
};

// register
int qvio_zdev_register(void);
void qvio_zdev_unregister(void);

// object alloc
struct qvio_zdev* qvio_zdev_new(void);
struct qvio_zdev* qvio_zdev_get(struct qvio_zdev* self);
void qvio_zdev_put(struct qvio_zdev* self);

// device probe
int qvio_zdev_probe(struct qvio_zdev* self);
void qvio_zdev_remove(struct qvio_zdev* self);

// device attrs
ssize_t qvio_zdev_attr_ver_show(struct qvio_zdev* self, char *buf);
ssize_t qvio_zdev_attr_ticks_show(struct qvio_zdev* self, char *buf);
ssize_t qvio_zdev_attr_value0_show(struct qvio_zdev* self, char *buf);
ssize_t qvio_zdev_attr_value0_store(struct qvio_zdev* self, const char *buf, size_t count);

// ioctl
int qvio_zdev_reset_mask(struct qvio_zdev* self, int reset_mask, unsigned int msecs);

#endif // __QVIO_ZDEV_H__