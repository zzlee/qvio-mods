#ifndef __QVIO_VIDEO_QUEUE_H__
#define __QVIO_VIDEO_QUEUE_H__

#include <linux/platform_device.h>

#include "uapi/qvio-l4t.h"
#include "buf_entry.h"

enum qvio_video_queue_state {
	QVIO_VIDEO_QUEUE_STATE_READY,
	QVIO_VIDEO_QUEUE_STATE_START
};

struct qvio_video_queue {
	struct kref ref;

	struct device *dev;
	uint32_t device_id;

	spinlock_t lock;
	struct list_head job_list; // qvio_buf_entry
	struct list_head done_list; // qvio_buf_entry
	enum qvio_video_queue_state state;

	// irq control
	wait_queue_head_t irq_wait;

	struct qvio_format format;
	int planes;
	__u32 buffers_count;
	struct qvio_buffer* buffers;
	size_t buffer_size;

	// for mmap
	void* mmap_buffer;
	size_t mmap_buffer_size;

	// events
	void* parent;
	int (*buf_entry_from_sgt)(struct qvio_video_queue* self, struct sg_table* sgt, struct qvio_buffer* buf, struct qvio_buf_entry* buf_entry);
	int (*start_buf_entry)(struct qvio_video_queue* self, struct qvio_buf_entry* buf_entry);
	int (*streamon)(struct qvio_video_queue* self);
	int (*streamoff)(struct qvio_video_queue* self);
};

// object alloc
struct qvio_video_queue* qvio_video_queue_new(void);
struct qvio_video_queue* qvio_video_queue_get(struct qvio_video_queue* self);
void qvio_video_queue_put(struct qvio_video_queue* self);

// file ops
__poll_t qvio_video_queue_file_poll(struct qvio_video_queue* self, struct file *filp, struct poll_table_struct *wait);
long qvio_video_queue_file_ioctl(struct qvio_video_queue* self, struct file * filp, unsigned int cmd, unsigned long arg);

int qvio_video_queue_done(struct qvio_video_queue* self, struct qvio_buf_entry** next_entry);

#endif // __QVIO_VIDEO_QUEUE_H__