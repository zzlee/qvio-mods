#ifndef __QVIO_DMA_BLOCK_H__
#define __QVIO_DMA_BLOCK_H__

#include <linux/dmapool.h>

struct dma_block_t {
	void *cpu_addr;
	dma_addr_t dma_handle;
};

int qvio_dma_block_alloc(struct dma_block_t* self, struct dma_pool* pool, gfp_t flags);
int qvio_dma_block_free(struct dma_block_t* self, struct dma_pool* pool);

#endif // __QVIO_DMA_BLOCK_H__
