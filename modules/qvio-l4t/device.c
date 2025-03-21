#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "device.h"

struct qvio_device* qvio_device_new(void) {
	int err;
	struct qvio_device* self = kzalloc(sizeof(struct qvio_device), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	spin_lock_init(&self->job_list_lock);
	INIT_LIST_HEAD(&self->job_list);

	spin_lock_init(&self->done_list_lock);
	INIT_LIST_HEAD(&self->done_list);

	return self;

err0:
	return NULL;
}

struct qvio_device* qvio_device_get(struct qvio_device* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __device_free(struct kref *ref)
{
	struct qvio_device* self = container_of(ref, struct qvio_device, ref);

	pr_info("\n");

	kfree(self);
}

void qvio_device_put(struct qvio_device* self) {
	if (self)
		kref_put(&self->ref, __device_free);
}
