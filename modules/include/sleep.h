/******************************************************************************
*
 *
 * Copyright (C) 2015, 2016, 2017 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
*
******************************************************************************/

#ifndef SLEEP_H
#define SLEEP_H

#include <linux/delay.h>
#include "xil_types.h"
#include "xil_io.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int usleep(unsigned long useconds) {
	usleep_range(useconds, useconds + useconds/10);

	return 0;
}

static inline unsigned sleep(unsigned int seconds) {
	usleep(seconds * 1000000);

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
