#include "dma_block.h"

int qvio_dma_block_alloc(struct dma_block_t* self, struct dma_pool* pool, gfp_t flags) {
	int ret;

	self->cpu_addr = dma_pool_alloc(pool, flags, &self->dma_handle);
	if(! self->cpu_addr) {
		pr_err("dma_pool_alloc() failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	return 0;

err0:
	return ret;
}

int qvio_dma_block_free(struct dma_block_t* self, struct dma_pool* pool) {
	dma_pool_free(pool, self->cpu_addr, self->dma_handle);

	return 0;
}
