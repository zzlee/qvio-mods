/*
 * QVIO Userspace API
 */
#ifndef _UAPI_LINUX_QVIO_L4T_H
#define _UAPI_LINUX_QVIO_L4T_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>

struct qvio_umods_fd {
	int flags; // O_RDONLY | O_CLOEXEC
	int fd;
};

enum qvio_umods_job_id {
	QVIO_UMODS_JOB_ID_S_FMT,
	QVIO_UMODS_JOB_ID_QUEUE_SETUP,
	QVIO_UMODS_JOB_ID_BUF_INIT,
	QVIO_UMODS_JOB_ID_BUF_PREPARE,
	QVIO_UMODS_JOB_ID_BUF_QUEUE,
	QVIO_UMODS_JOB_ID_BUF_FINISH,
	QVIO_UMODS_JOB_ID_BUF_CLEANUP,
	QVIO_UMODS_JOB_ID_START_STREAMING,
	QVIO_UMODS_JOB_ID_STOP_STREAMING,
};

struct qvio_umods_req {
	__u16 job_id; // qvio_umods_job_id
	__u16 sequence;

	union {
		struct {
			struct v4l2_format format;
		} s_fmt;

		struct {
			unsigned int num_buffers;
			unsigned int num_planes;
			unsigned int sizes[VIDEO_MAX_PLANES];
		} queue_setup;

		struct {
			int flags;
		} buf_init;

		struct {
			int flags;
		} buf_prepare;

		struct {
			int flags;
		} buf_queue;

		struct {
			int flags;
		} buf_finish;

		struct {
			int flags;
		} buf_cleanup;

		struct {
			int flags;
		} start_streaming;

		struct {
			int flags;
		} stop_streaming;
	} u;
};

struct qvio_umods_rsp {
	__u16 job_id; // qvio_umods_job_id
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
		} buf_init;

		struct {
			int flags;
		} buf_prepare;

		struct {
			int flags;
		} buf_queue;

		struct {
			int flags;
		} buf_finish;

		struct {
			int flags;
		} buf_cleanup;

		struct {
			int flags;
		} start_streaming;

		struct {
			int flags;
		} stop_streaming;
	} u;
};

struct qvio_umods_rsp_req {
	struct qvio_umods_rsp rsp;
	struct qvio_umods_req req;
};

#define QVID_IOC_MAGIC		'Q'

// qvio v4l2 ioctls
#define QVID_IOC_G_UMODS_FD			_IOWR(QVID_IOC_MAGIC, BASE_VIDIOC_PRIVATE+0, struct qvio_umods_fd)
#define QVID_IOC_BUF_DONE			_IO  (QVID_IOC_MAGIC, BASE_VIDIOC_PRIVATE+1)
#define QVID_IOC_S_BUF_DONE			_IOWR(QVID_IOC_MAGIC, BASE_VIDIOC_PRIVATE+0, struct qvio_umods_fd)

// UMODS_FD ioctls
#define QVID_IOC_G_UMODS_REQ		_IOWR(QVID_IOC_MAGIC, 1, struct qvio_umods_req)
#define QVID_IOC_S_UMODS_RSP		_IOWR(QVID_IOC_MAGIC, 2, struct qvio_umods_rsp)
#define QVID_IOC_S_UMODS_RSP_REQ	_IOWR(QVID_IOC_MAGIC, 3, struct qvio_umods_rsp_req)

#endif /* _UAPI_LINUX_QVIO_L4T_H */
