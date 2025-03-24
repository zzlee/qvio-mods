#ifndef __QVIO_BUF_ENTRY_H__
#define __QVIO_BUF_ENTRY_H__

#include <linux/kref.h>
#include <linux/dma-buf.h>

#include "dma_block.h"
#include "uapi/qvio-l4t.h"

struct qvio_buf_entry {
	struct kref ref;
	struct list_head node;
	struct qvio_buffer buf;

	struct device* dev;
	enum dma_data_direction dma_dir;

	union {
		struct {
			struct dma_buf *dmabuf;
			struct dma_buf_attachment *attach;
			struct sg_table *sgt;
		} dmabuf;

		struct {
			struct sg_table* sgt;
		} userptr;

		struct {
			struct sg_table* sgt;
		} mmap;
	} u;

	// vars for descriptors building
	struct dma_pool* desc_pool;
	struct dma_block_t desc_blocks[8];
	dma_addr_t desc_dma_handle;
	u32 desc_items;
	dma_addr_t dst_dma_handle;
};

struct qvio_buf_entry* qvio_buf_entry_new(void);
struct qvio_buf_entry* qvio_buf_entry_get(struct qvio_buf_entry* self);
void qvio_buf_entry_put(struct qvio_buf_entry* self);

#endif // __QVIO_BUF_ENTRY_H__