#ifndef __QDMABUF_DEVICE_H__
#define __QDMABUF_DEVICE_H__

#include "cdev.h"

#define QDMABUF_DRIVER_NAME "qdmabuf"

struct qdmabuf_device {
	struct kref ref;
	struct platform_device* pdev;

	// cdev
	struct qdmabuf_cdev cdev;
};

int qdmabuf_device_register(void);
void qdmabuf_device_unregister(void);

#endif // __QDMABUF_DEVICE_H__