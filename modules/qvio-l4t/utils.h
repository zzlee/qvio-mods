#ifndef __QVIO_UTILS_H__
#define __QVIO_UTILS_H__

#include <linux/types.h>
#include <linux/io.h>
#include <linux/scatterlist.h>

#include "uapi/qvio-l4t.h"

enum {
	FOURCC_Y800 = 0x30303859,
	FOURCC_NV12 = 0x3231564E,
	FOURCC_NV16 = 0x3631564E,
};

static inline uint32_t fourcc(char a, char b, char c, char d) {
	return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

static inline void io_write_reg(uintptr_t BaseAddress, int RegOffset, u32 Data) {
    iowrite32(Data, (volatile void *)(BaseAddress + RegOffset));
}

static inline u32 io_read_reg(uintptr_t BaseAddress, int RegOffset) {
    return ioread32((const volatile void *)(BaseAddress + RegOffset));
}

ssize_t utils_calc_buf_size(struct qvio_format* format, __u32 offset[4], __u32 stride[4]);
void utils_sgt_dump(struct sg_table *sgt, bool full);

#endif // __QVIO_UTILS_H__
