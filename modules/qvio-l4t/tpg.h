#ifndef __QVIO_TPG_H__
#define __QVIO_TPG_H__

#include <linux/platform_device.h>

#include "cdev.h"
#include "zdev.h"
#include "xv_tpg.h"
#include "uapi/qvio-l4t.h"

struct qvio_tpg {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;
	struct qvio_cdev cdev;

	struct qvio_zdev* zdev;
	void __iomem * reg;
	XV_tpg xtpg;

	struct qvio_format format;
	struct qvio_tpg_config cur_config;
	u32 nXtpgColorFormat;
};

// register
int qvio_tpg_register(void);
void qvio_tpg_unregister(void);

// object alloc
struct qvio_tpg* qvio_tpg_new(void);
struct qvio_tpg* qvio_tpg_get(struct qvio_tpg* self);
void qvio_tpg_put(struct qvio_tpg* self);

// device probe
int qvio_tpg_probe(struct qvio_tpg* self);
void qvio_tpg_remove(struct qvio_tpg* self);

#endif // __QVIO_TPG_H__
