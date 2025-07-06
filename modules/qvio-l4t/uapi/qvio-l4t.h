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
			unsigned int index;
			unsigned int type;
			unsigned int memory;
			__u64 timestamp;
		} buf_init;

		struct {
			unsigned int index;
			unsigned int type;
			unsigned int memory;
			__u64 timestamp;
		} buf_prepare;

		struct {
			unsigned int index;
			unsigned int type;
			unsigned int memory;
			__u64 timestamp;
		} buf_queue;

		struct {
			unsigned int index;
			unsigned int type;
			unsigned int memory;
			__u64 timestamp;
		} buf_finish;

		struct {
			unsigned int index;
			unsigned int type;
			unsigned int memory;
			__u64 timestamp;
		} buf_cleanup;

		struct {
			unsigned int count;
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

struct qvio_g_ticks {
	__u32 ticks;
};

struct qvio_format {
	__u32 width;
	__u32 height;
	__u32 fmt; // fourcc
};

enum qvio_buf_type {
	QVIO_BUF_TYPE_USERPTR = 1,
	QVIO_BUF_TYPE_DMABUF,
	QVIO_BUF_TYPE_MMAP,
};

enum qvio_buf_dir {
	QVIO_BUF_DIR_BIDIRECTIONAL = 0,
	QVIO_BUF_DIR_TO_DEVICE = 1,
	QVIO_BUF_DIR_FROM_DEVICE = 2,
	QVIO_BUF_DIR_NONE = 3,
};

enum qvio_work_mode {
	QVIO_WORK_MODE_AXIMM_TEST0 = 1,
	QVIO_WORK_MODE_AXIMM_TEST1,
	QVIO_WORK_MODE_FRMBUFWR,
	QVIO_WORK_MODE_AXIMM_TEST2,
	QVIO_WORK_MODE_AXIMM_TEST3,
	QVIO_WORK_MODE_AXIMM_TEST2_1,
	QVIO_WORK_MODE_XDMA_H2C,
	QVIO_WORK_MODE_XDMA_C2H,
	QVIO_WORK_MODE_QDMA_WR,
	QVIO_WORK_MODE_QDMA_RD,
};

struct qvio_buffer {
	__u32 index;

	__u16 buf_type; // ref to qvio_buf_type
	__u16 buf_dir; // ref to qvio_buf_dir
	union {
		unsigned long userptr;
		__s32 fd;
		__u32 offset;
	} u;

	__u32 offset[4];
	__u32 stride[4];
};

struct qvio_req_bufs {
	__u32 count;

	__u16 buf_type; // ref to qvio_buf_type

	__u32 offset[4];
	__u32 stride[4];
};

#define QVIO_IOC_MAGIC		'Q'

// qvio v4l2 ioctls
#define QVID_IOC_G_UMODS_FD			_IOWR(QVIO_IOC_MAGIC, BASE_VIDIOC_PRIVATE+0, struct qvio_umods_fd)
#define QVID_IOC_BUF_DONE			_IO  (QVIO_IOC_MAGIC, BASE_VIDIOC_PRIVATE+1)
#define QVID_IOC_S_BUF_DONE			_IOWR(QVIO_IOC_MAGIC, BASE_VIDIOC_PRIVATE+0, struct qvio_umods_fd)

// UMODS_FD ioctls
#define QVID_IOC_G_UMODS_REQ		_IOWR(QVIO_IOC_MAGIC, 1, struct qvio_umods_req)
#define QVID_IOC_S_UMODS_RSP		_IOWR(QVIO_IOC_MAGIC, 2, struct qvio_umods_rsp)

// qvio ioctls
#define QVIO_IOC_G_TICKS		_IOR (QVIO_IOC_MAGIC, 0x1, struct qvio_g_ticks)
#define QVIO_IOC_S_FMT			_IOW (QVIO_IOC_MAGIC, 0x2, struct qvio_format)
#define QVIO_IOC_G_FMT			_IOR (QVIO_IOC_MAGIC, 0x2, struct qvio_format)
#define QVIO_IOC_REQ_BUFS		_IOWR(QVIO_IOC_MAGIC, 0x3, struct qvio_req_bufs)
#define QVIO_IOC_QUERY_BUF		_IOWR(QVIO_IOC_MAGIC, 0x4, struct qvio_buffer)
#define QVIO_IOC_QBUF			_IOWR(QVIO_IOC_MAGIC, 0x5, struct qvio_buffer)
#define QVIO_IOC_DQBUF			_IOWR(QVIO_IOC_MAGIC, 0x6, struct qvio_buffer)
#define QVIO_IOC_STREAMON		_IO  (QVIO_IOC_MAGIC, 0x7)
#define QVIO_IOC_STREAMOFF		_IO  (QVIO_IOC_MAGIC, 0x8)
#define QVIO_IOC_S_WORK_MODE	_IOW (QVIO_IOC_MAGIC, 0x9, int)
#define QVIO_IOC_G_WORK_MODE	_IOR (QVIO_IOC_MAGIC, 0x9, int)

#endif /* _UAPI_LINUX_QVIO_L4T_H */
