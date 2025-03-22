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

	self->done_jobs = 0;
	init_waitqueue_head(&self->irq_wait);
	atomic_set(&self->irq_event, 0);
	self->irq_event_count = atomic_read(&self->irq_event);
	atomic_set(&self->irq_event_data, 0);

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
	struct qvio_buf_entry* buf_entry;

	pr_info("\n");

	if(! list_empty(&self->job_list)) {
		pr_warn("job list is not empty!!\n");

		while(! list_empty(&self->job_list)) {
			buf_entry = list_first_entry(&self->job_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}

	if(! list_empty(&self->done_list)) {
		pr_warn("done list is not empty!!\n");

		while(! list_empty(&self->done_list)) {
			buf_entry = list_first_entry(&self->done_list, struct qvio_buf_entry, node);
			list_del(&buf_entry->node);
			qvio_buf_entry_put(buf_entry);
		}
	}

	kfree(self);
}

void qvio_device_put(struct qvio_device* self) {
	if (self)
		kref_put(&self->ref, __device_free);
}
