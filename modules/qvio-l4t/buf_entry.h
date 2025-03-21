#ifndef __QVIO_BUF_ENTRY_H__
#define __QVIO_BUF_ENTRY_H__

#include <linux/kref.h>

#include "uapi/qvio-l4t.h"

struct qvio_buf_entry {
	struct kref ref;
	struct list_head node;
	struct qvio_qbuf qbuf;
};

struct qvio_buf_entry* qvio_buf_entry_new(void);
struct qvio_buf_entry* qvio_buf_entry_get(struct qvio_buf_entry* self);
void qvio_buf_entry_put(struct qvio_buf_entry* self);

#endif // __QVIO_BUF_ENTRY_H__