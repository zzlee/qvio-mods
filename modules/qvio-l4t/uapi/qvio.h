/*
 * QVIO Userspace API
 */
#ifndef _UAPI_LINUX_QVIO_H
#define _UAPI_LINUX_QVIO_H

enum qvio_user_job_id {
	QVIO_USER_JOB_ID_S_FMT,
	QVIO_USER_JOB_ID_QUEUE_SETUP,
	QVIO_USER_JOB_ID_BUF_INIT,
	QVIO_USER_JOB_ID_BUF_CLEANUP,
	QVIO_USER_JOB_ID_START_STREAMING,
	QVIO_USER_JOB_ID_STOP_STREAMING,
	QVIO_USER_JOB_ID_BUF_DONE,
};

struct qvio_user_job {
	__u16 id;
	__u16 sequence;

	union {
		struct {
			struct v4l2_format format;
		} s_fmt;

		struct {
			unsigned int num_buffers;
		} queue_setup;

		struct {
			int index;
		} buf_init;

		struct {
			int index;
		} buf_cleanup;

		struct {
			int flags;
		} start_streaming;

		struct {
			int flags;
		} stop_streaming;

		struct {
			int index;
		} buf_done;
	} u;
};

struct qvio_user_job_done {
	__u16 id;
	__u16 sequence;

	union {
		struct {
			int flags;
		} s_fmt;

		struct {
			int flags;
		} queue_setup;

		struct {
			int flags;
			int dma_buf;
			int offset[4];
			int pitch[4];
			int psize[4];
		} buf_init;

		struct {
			int flags;
		} buf_cleanup;

		struct {
			int flags;
		} start_streaming;

		struct {
			int flags;
		} stop_streaming;

		struct {
			int flags;
		} buf_done;
	} u;
};

#define QVID_IOC_MAGIC		'Q'

// qvio cdev ioctls
#define QVID_IOC_IOCOFFLINE		_IO  (QVID_IOC_MAGIC, 1)
#define QVID_IOC_IOCONLINE		_IO  (QVID_IOC_MAGIC, 2)

// qvio v4l2 ioctls
#define QVID_IOC_USER_JOB_FD	_IOR (QVID_IOC_MAGIC, BASE_VIDIOC_PRIVATE+0, int)
#define QVID_IOC_BUF_DONE		_IO  (QVID_IOC_MAGIC, BASE_VIDIOC_PRIVATE+1)

// USER_JOB_FD ioctls
#define QVID_IOC_USER_JOB_GET	_IOR (QVID_IOC_MAGIC, 1, struct qvio_user_job)
#define QVID_IOC_USER_JOB_DONE	_IOW (QVID_IOC_MAGIC, 2, struct qvio_user_job_done)

#endif /* _UAPI_LINUX_QVIO_H */
