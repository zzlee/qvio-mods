#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "buf_entry.h"

#include <linux/kernel.h>
#include <linux/slab.h>

static void __buf_entry_free(struct kref *ref);

struct qvio_buf_entry* qvio_buf_entry_new(void) {
	struct qvio_buf_entry* self;

	self = kzalloc(sizeof(struct qvio_buf_entry), GFP_KERNEL);
	if(! self) {
		pr_err("out of memory\n");
		goto err0;
	}

	kref_init(&self->ref);
	INIT_LIST_HEAD(&self->node);

	// pr_info("self=%px\n", self);

	return self;

err0:
	return self;
}

struct qvio_buf_entry* qvio_buf_entry_get(struct qvio_buf_entry* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

void qvio_buf_entry_put(struct qvio_buf_entry* self) {
	if (self)
		kref_put(&self->ref, __buf_entry_free);
}

static void __buf_entry_free(struct kref *ref) {
	struct qvio_buf_entry* self = container_of(ref, struct qvio_buf_entry, ref);

	// pr_info("self=%px\n", self);

	kfree(self);
}
