/*
 * QDMABUF Userspace API
 */
#ifndef _UAPI_LINUX_QDMABUF_H
#define _UAPI_LINUX_QDMABUF_H

#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * DOC: QDMABUF Userspace API
 */

#define QDMABUF_TYPE_DMA_CONTIG		0x00
#define QDMABUF_TYPE_DMA_SG			0x01
#define QDMABUF_TYPE_VMALLOC		0x02

#define QDMABUF_VALID_FD_FLAGS 	(O_CLOEXEC | O_ACCMODE)

#define QDMABUF_DMA_DIR_BIDIRECTIONAL	0
#define QDMABUF_DMA_DIR_TO_DEVICE		1
#define QDMABUF_DMA_DIR_FROM_DEVICE		2
#define QDMABUF_DMA_DIR_NONE			3

/**
 * struct qdmabuf_alloc_args - metadata passed from userspace for
 *                                      allocations
 *
 * Provided by userspace as an argument to the ioctl
 */
struct qdmabuf_alloc_args {
	__u64 len;
	__u32 type;
	__u32 fd_flags;
	__u32 dma_dir;
	__u32 fd;
};

struct qdmabuf_info_args {
	__u32 fd;
};

#define QDMABUF_IOC_MAGIC		'Q'

#define QDMABUF_IOCTL_ALLOC		_IOWR(QDMABUF_IOC_MAGIC, 0x0, struct qdmabuf_alloc_args)
#define QDMABUF_IOCTL_INFO		_IOWR(QDMABUF_IOC_MAGIC, 0x1, struct qdmabuf_info_args)

#endif /* _UAPI_LINUX_QDMABUF_H */
