#ifndef __QVIO_USER_JOB_H__
#define __QVIO_USER_JOB_H__

#include <linux/wait.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>

#include "uapi/qvio.h"

typedef void (*qvio_user_job_done_handler)(void* user, struct qvio_user_job_done* user_job_done);

struct qvio_user_job_ctrl {
	bool enable;
	atomic_t sequence;

	// user-job list
	wait_queue_head_t job_wq;
	spinlock_t job_list_lock;
	struct list_head job_list;

	// user-job-done lsit
	wait_queue_head_t done_wq;
	spinlock_t done_list_lock;
	struct list_head done_list;

	// user-job control
	const struct file_operations* ctrl_fops;
};

void qvio_user_job_start(struct qvio_user_job_ctrl* self);
void qvio_user_job_stop(struct qvio_user_job_ctrl* self);

// user-job
int qvio_user_job_s_fmt(struct qvio_user_job_ctrl* self, struct v4l2_format *format);
int qvio_user_job_queue_setup(struct qvio_user_job_ctrl* self, unsigned int num_buffers);
int qvio_user_job_buf_init(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer, void* user, qvio_user_job_done_handler fn);
int qvio_user_job_buf_cleanup(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer);
int qvio_user_job_start_streaming(struct qvio_user_job_ctrl* self);
int qvio_user_job_stop_streaming(struct qvio_user_job_ctrl* self);
int qvio_user_job_buf_done(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer);

#endif // __QVIO_USER_JOB_H__