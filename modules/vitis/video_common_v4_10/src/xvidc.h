/*******************************************************************************
* Copyright (C) 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
*******************************************************************************/

/******************************************************************************/
/**
 *
 * @file xvidc.h
 * @addtogroup video_common_v4_10
 * @{
 * @details
 *
 * Contains common structures, definitions, macros, and utility functions that
 * are typically used by video-related drivers and applications.
 *
 * @note	None.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -----------------------------------------------
 * 1.0   rc,  01/10/15 Initial release.
 *       als
 * 2.0   als  08/14/15 Added new video timings.
 * 2.2   als  02/01/16 Functions with pointer arguments that don't modify
 *                     contents now const.
 *                     Added ability to insert a custom video timing table:
 *                         XVidC_RegisterCustomTimingModes
 *                         XVidC_UnregisterCustomTimingMode
 *       yh            Added 3D support.
 * 3.0   aad  05/13/16 Added API to search for RB video modes.
 *       als  05/16/16 Added Y-only to color format enum.
 * 3.1   rco  07/26/17 Moved timing table extern definition to xvidc.c
 *                     Added video-in-memory color formats
 *                     Updated XVidC_RegisterCustomTimingModes API signature
 * 4.1   rco  11/23/17 Added new memory formats
 *                     Added xil_printf include statement
 *                     Added new API XVidC_GetVideoModeIdWBlanking
 *                     Fix C++ warnings
 * 4.2   jsr  07/22/17 Added new video modes, framerates, color formats for SDI
 *                     New member AspectRatio is added to video stream structure
 *                     Reordered XVidC_VideoMode enum variables and corrected the
 *                     memory format enums
 *       aad  07/10/17 Add XVIDC_VM_3840x2160_60_P_RB video format
 *       vyc  10/04/17 Added new streaming alpha formats and new memory formats
 *       aad  09/05/17 Add XVIDC_VM_1366x768_60_P_RB resolution
 * 4.3   eb   26/01/18 Added API XVidC_GetVideoModeIdExtensive
 *       jsr  02/22/18 Added XVIDC_CSF_YCBCR_420 color space format
 *       vyc  04/04/18 Added BGR8 memory format
 * 4.5   jsr  07/03/18 Added XVIDC_VM_720x486_60_I video format
 * 4.5   yas  03/08/19 Added support for frame rates 144HZ and 240HZ
 * 4.6   mmo  02/14/19 Added 5k, 8k, 10k and Low Resolution with 200Hz, 240Hz
 * </pre>
 *
*******************************************************************************/

#ifndef XVIDC_H_  /* Prevent circular inclusions by using protection macros. */
#define XVIDC_H_

#ifdef __cplusplus
extern "C" {
#endif

/******************************* Include Files ********************************/

#include "xil_types.h"
#include "xil_printf.h"

/************************** Constant Definitions ******************************/

/**
 * This typedef enumerates the list of available standard display monitor
 * timings as specified in the xvidc_timings_table.c file. The naming format is:
 *
 * XVIDC_VM_<RESOLUTION>_<REFRESH RATE (HZ)>_<P|I>(_RB)
 *
 * Where RB stands for reduced blanking.
 */
typedef enum {
	/* Interlaced modes. */
	XVIDC_VM_720x480_60_I = 0,
	XVIDC_VM_720x486_60_I,
	XVIDC_VM_720x576_50_I,
	XVIDC_VM_1440x480_60_I,
	XVIDC_VM_1440x480_120_I,
	XVIDC_VM_1440x480_240_I,
	XVIDC_VM_1440x576_50_I,
	XVIDC_VM_1440x576_100_I,
	XVIDC_VM_1440x576_200_I,
	XVIDC_VM_1920x1080_48_I,
	XVIDC_VM_1920x1080_50_I,
	XVIDC_VM_1920x1080_60_I,
	XVIDC_VM_1920x1080_96_I,
	XVIDC_VM_1920x1080_100_I,
	XVIDC_VM_1920x1080_120_I,
	XVIDC_VM_2048x1080_48_I,
	XVIDC_VM_2048x1080_50_I,
	XVIDC_VM_2048x1080_60_I,
	XVIDC_VM_2048x1080_96_I,
	XVIDC_VM_2048x1080_100_I,
	XVIDC_VM_2048x1080_120_I,
	XVIDC_VM_2880x480_60_I,
	XVIDC_VM_2880x576_50_I,


	/* Progressive modes. */
	XVIDC_VM_640x350_85_P,
	XVIDC_VM_640x480_60_P,
	XVIDC_VM_640x480_72_P,
	XVIDC_VM_640x480_75_P,
	XVIDC_VM_640x480_85_P,
	XVIDC_VM_720x400_85_P,
	XVIDC_VM_720x480_60_P,
	XVIDC_VM_720x480_120_P,
	XVIDC_VM_720x480_240_P,
	XVIDC_VM_720x576_50_P,
	XVIDC_VM_720x576_100_P,
	XVIDC_VM_720x576_200_P,
	XVIDC_VM_800x600_56_P,
	XVIDC_VM_800x600_60_P,
	XVIDC_VM_800x600_72_P,
	XVIDC_VM_800x600_75_P,
	XVIDC_VM_800x600_85_P,
	XVIDC_VM_800x600_120_P_RB,
	XVIDC_VM_848x480_60_P,
	XVIDC_VM_1024x768_60_P,
	XVIDC_VM_1024x768_70_P,
	XVIDC_VM_1024x768_75_P,
	XVIDC_VM_1024x768_85_P,
	XVIDC_VM_1024x768_120_P_RB,
	XVIDC_VM_1152x864_75_P,
	XVIDC_VM_1280x720_24_P,
	XVIDC_VM_1280x720_25_P,
	XVIDC_VM_1280x720_30_P,
	XVIDC_VM_1280x720_50_P,
	XVIDC_VM_1280x720_60_P,
	XVIDC_VM_1280x720_100_P,
	XVIDC_VM_1280x720_120_P,
	XVIDC_VM_1280x768_60_P,
	XVIDC_VM_1280x768_60_P_RB,
	XVIDC_VM_1280x768_75_P,
	XVIDC_VM_1280x768_85_P,
	XVIDC_VM_1280x768_120_P_RB,
	XVIDC_VM_1280x800_60_P,
	XVIDC_VM_1280x800_60_P_RB,
	XVIDC_VM_1280x800_75_P,
	XVIDC_VM_1280x800_85_P,
	XVIDC_VM_1280x800_120_P_RB,
	XVIDC_VM_1280x960_60_P,
	XVIDC_VM_1280x960_85_P,
	XVIDC_VM_1280x960_120_P_RB,
	XVIDC_VM_1280x1024_60_P,
	XVIDC_VM_1280x1024_75_P,
	XVIDC_VM_1280x1024_85_P,
	XVIDC_VM_1280x1024_120_P_RB,
	XVIDC_VM_1360x768_60_P,
	XVIDC_VM_1360x768_120_P_RB,
	XVIDC_VM_1366x768_60_P,
	XVIDC_VM_1366x768_60_P_RB,
	XVIDC_VM_1400x1050_60_P,
	XVIDC_VM_1400x1050_60_P_RB,
	XVIDC_VM_1400x1050_75_P,
	XVIDC_VM_1400x1050_85_P,
	XVIDC_VM_1400x1050_120_P_RB,
	XVIDC_VM_1440x240_60_P,
	XVIDC_VM_1440x288_50_P,
	XVIDC_VM_1440x480_60_P,
	XVIDC_VM_1440x576_50_P,
	XVIDC_VM_1440x900_60_P,
	XVIDC_VM_1440x900_60_P_RB,
	XVIDC_VM_1440x900_75_P,
	XVIDC_VM_1440x900_85_P,
	XVIDC_VM_1440x900_120_P_RB,
	XVIDC_VM_1600x1200_30_P,	
	XVIDC_VM_1600x1200_60_P,
	XVIDC_VM_1600x1200_65_P,
	XVIDC_VM_1600x1200_70_P,
	XVIDC_VM_1600x1200_75_P,
	XVIDC_VM_1600x1200_85_P,
	XVIDC_VM_1600x1200_120_P_RB,
	XVIDC_VM_1680x720_24_P,
	XVIDC_VM_1680x720_25_P,
	XVIDC_VM_1680x720_30_P,
	XVIDC_VM_1680x720_50_P,
	XVIDC_VM_1680x720_60_P,
	XVIDC_VM_1680x720_100_P,
	XVIDC_VM_1680x720_120_P,
	XVIDC_VM_1680x1050_50_P,
	XVIDC_VM_1680x1050_60_P,
	XVIDC_VM_1680x1050_60_P_RB,
	XVIDC_VM_1680x1050_75_P,
	XVIDC_VM_1680x1050_85_P,
	XVIDC_VM_1680x1050_120_P_RB,
	XVIDC_VM_1792x1344_60_P,
	XVIDC_VM_1792x1344_75_P,
	XVIDC_VM_1792x1344_120_P_RB,
	XVIDC_VM_1856x1392_60_P,
	XVIDC_VM_1856x1392_75_P,
	XVIDC_VM_1856x1392_120_P_RB,
	XVIDC_VM_1920x1080_24_P,
	XVIDC_VM_1920x1080_25_P,
	XVIDC_VM_1920x1080_30_P,
	XVIDC_VM_1920x1080_48_P,
	XVIDC_VM_1920x1080_50_P,
	XVIDC_VM_1920x1080_60_P,
	XVIDC_VM_1920x1080_100_P,
	XVIDC_VM_1920x1080_120_P,
	XVIDC_VM_1920x1200_30_P,	
	XVIDC_VM_1920x1200_60_P,
	XVIDC_VM_1920x1200_60_P_RB,
	XVIDC_VM_1920x1200_75_P,
	XVIDC_VM_1920x1200_85_P,
	XVIDC_VM_1920x1200_120_P_RB,
	XVIDC_VM_1920x1440_60_P,
	XVIDC_VM_1920x1440_75_P,
	XVIDC_VM_1920x1440_120_P_RB,
	XVIDC_VM_1920x2160_60_P,
	XVIDC_VM_2048x1080_24_P,
	XVIDC_VM_2048x1080_25_P,
	XVIDC_VM_2048x1080_30_P,
	XVIDC_VM_2048x1080_48_P,
	XVIDC_VM_2048x1080_50_P,
	XVIDC_VM_2048x1080_60_P,
	XVIDC_VM_2048x1080_100_P,
	XVIDC_VM_2048x1080_120_P,
	XVIDC_VM_2560x1080_24_P,
	XVIDC_VM_2560x1080_25_P,
	XVIDC_VM_2560x1080_30_P,
	XVIDC_VM_2560x1080_48_P,
	XVIDC_VM_2560x1080_50_P,
	XVIDC_VM_2560x1080_60_P,
	XVIDC_VM_2560x1080_100_P,
	XVIDC_VM_2560x1080_120_P,
	XVIDC_VM_2560x1440_30_P,
	XVIDC_VM_2560x1440_60_P,	
	XVIDC_VM_2560x1600_60_P,
	XVIDC_VM_2560x1600_60_P_RB,
	XVIDC_VM_2560x1600_75_P,
	XVIDC_VM_2560x1600_85_P,
	XVIDC_VM_2560x1600_120_P_RB,
	XVIDC_VM_2880x240_60_P,
	XVIDC_VM_2880x288_50_P,
	XVIDC_VM_2880x480_60_P,
	XVIDC_VM_2880x576_50_P,
	XVIDC_VM_3440x1440_30_P,
	XVIDC_VM_3440x1440_60_P,
	XVIDC_VM_3840x1600_30_P,
	XVIDC_VM_3840x1600_60_P,		
	XVIDC_VM_3840x2160_24_P,
	XVIDC_VM_3840x2160_25_P,
	XVIDC_VM_3840x2160_30_P,
	XVIDC_VM_3840x2160_48_P,
	XVIDC_VM_3840x2160_50_P,
	XVIDC_VM_3840x2160_60_P,
	XVIDC_VM_3840x2160_60_P_RB,
	XVIDC_VM_3840x2160_100_P,
	XVIDC_VM_3840x2160_120_P,
	XVIDC_VM_4096x2160_24_P,
	XVIDC_VM_4096x2160_25_P,
	XVIDC_VM_4096x2160_30_P,
	XVIDC_VM_4096x2160_48_P,
	XVIDC_VM_4096x2160_50_P,
	XVIDC_VM_4096x2160_60_P,
	XVIDC_VM_4096x2160_60_P_RB,
	XVIDC_VM_4096x2160_100_P,
	XVIDC_VM_4096x2160_120_P,
	XVIDC_VM_5120x2160_24_P,
	XVIDC_VM_5120x2160_25_P,
	XVIDC_VM_5120x2160_30_P,
	XVIDC_VM_5120x2160_48_P,
	XVIDC_VM_5120x2160_50_P,
	XVIDC_VM_5120x2160_60_P,
	XVIDC_VM_5120x2160_100_P,
	XVIDC_VM_5120x2160_120_P,
	XVIDC_VM_7680x4320_24_P,
	XVIDC_VM_7680x4320_25_P,
	XVIDC_VM_7680x4320_30_P,
	XVIDC_VM_7680x4320_48_P,
	XVIDC_VM_7680x4320_50_P,
	XVIDC_VM_7680x4320_60_P,
	XVIDC_VM_7680x4320_120_P,
	XVIDC_VM_7680x4320_100_P,
	XVIDC_VM_10240x4320_24_P,
	XVIDC_VM_10240x4320_25_P,
	XVIDC_VM_10240x4320_30_P,
	XVIDC_VM_10240x4320_48_P,
	XVIDC_VM_10240x4320_50_P,
	XVIDC_VM_10240x4320_60_P,
	XVIDC_VM_10240x4320_100_P,
	XVIDC_VM_10240x4320_120_P,

	XVIDC_VM_NUM_SUPPORTED,
	XVIDC_VM_USE_EDID_PREFERRED,
	XVIDC_VM_NO_INPUT,
	XVIDC_VM_NOT_SUPPORTED,
	XVIDC_VM_CUSTOM,

	/* Marks beginning/end of interlaced/progressive modes in the table. */
	XVIDC_VM_INTL_START = XVIDC_VM_720x480_60_I,
	XVIDC_VM_PROG_START = XVIDC_VM_640x350_85_P,
	XVIDC_VM_INTL_END = (XVIDC_VM_PROG_START - 1),
	XVIDC_VM_PROG_END = (XVIDC_VM_NUM_SUPPORTED - 1),

	/* Common naming. */
	XVIDC_VM_480_60_I = XVIDC_VM_720x480_60_I,
	XVIDC_VM_486_60_I = XVIDC_VM_720x486_60_I,
	XVIDC_VM_576_50_I = XVIDC_VM_720x576_50_I,
	XVIDC_VM_1080_50_I = XVIDC_VM_1920x1080_50_I,
	XVIDC_VM_1080_60_I = XVIDC_VM_1920x1080_60_I,
	XVIDC_VM_VGA_60_P = XVIDC_VM_640x480_60_P,
	XVIDC_VM_480_60_P = XVIDC_VM_720x480_60_P,
	XVIDC_VM_SVGA_60_P = XVIDC_VM_800x600_60_P,
	XVIDC_VM_XGA_60_P = XVIDC_VM_1024x768_60_P,
	XVIDC_VM_720_50_P = XVIDC_VM_1280x720_50_P,
	XVIDC_VM_720_60_P = XVIDC_VM_1280x720_60_P,
	XVIDC_VM_WXGA_60_P = XVIDC_VM_1366x768_60_P,
	XVIDC_VM_UXGA_60_P = XVIDC_VM_1600x1200_60_P,
	XVIDC_VM_WSXGA_60_P = XVIDC_VM_1680x1050_60_P,
	XVIDC_VM_1080_24_P = XVIDC_VM_1920x1080_24_P,
	XVIDC_VM_1080_25_P = XVIDC_VM_1920x1080_25_P,
	XVIDC_VM_1080_30_P = XVIDC_VM_1920x1080_30_P,
	XVIDC_VM_1080_50_P = XVIDC_VM_1920x1080_50_P,
	XVIDC_VM_1080_60_P = XVIDC_VM_1920x1080_60_P,
	XVIDC_VM_WUXGA_60_P = XVIDC_VM_1920x1200_60_P,
	XVIDC_VM_UHD2_60_P = XVIDC_VM_1920x2160_60_P,
	XVIDC_VM_UHD_24_P = XVIDC_VM_3840x2160_24_P,
	XVIDC_VM_UHD_25_P = XVIDC_VM_3840x2160_25_P,
	XVIDC_VM_UHD_30_P = XVIDC_VM_3840x2160_30_P,
	XVIDC_VM_UHD_60_P = XVIDC_VM_3840x2160_60_P,
	XVIDC_VM_4K2K_60_P = XVIDC_VM_4096x2160_60_P,
	XVIDC_VM_4K2K_60_P_RB = XVIDC_VM_4096x2160_60_P_RB,
} XVidC_VideoMode;

/**
 * Progressive/interlaced video format.
 */
typedef enum {
	XVIDC_VF_PROGRESSIVE = 0,
	XVIDC_VF_INTERLACED,
	XVIDC_VF_UNKNOWN
} XVidC_VideoFormat;

/**
 * Frame rate.
 */
typedef enum {
	XVIDC_FR_24HZ = 24,
	XVIDC_FR_25HZ = 25,
	XVIDC_FR_30HZ = 30,
	XVIDC_FR_48HZ = 48,
	XVIDC_FR_50HZ = 50,
	XVIDC_FR_56HZ = 56,
	XVIDC_FR_60HZ = 60,
	XVIDC_FR_65HZ = 65,
	XVIDC_FR_67HZ = 67,
	XVIDC_FR_70HZ = 70,
	XVIDC_FR_72HZ = 72,
	XVIDC_FR_75HZ = 75,
	XVIDC_FR_85HZ = 85,
	XVIDC_FR_87HZ = 87,
	XVIDC_FR_88HZ = 88,
	XVIDC_FR_96HZ = 96,
	XVIDC_FR_100HZ = 100,
	XVIDC_FR_120HZ = 120,
	XVIDC_FR_144HZ = 144,
	XVIDC_FR_200HZ = 200,
	XVIDC_FR_240HZ = 240,
	XVIDC_FR_NUM_SUPPORTED = 21,
	XVIDC_FR_UNKNOWN
} XVidC_FrameRate;

/**
 * Color depth - bits per color component.
 */
typedef enum {
	XVIDC_BPC_6 = 6,
	XVIDC_BPC_8 = 8,
	XVIDC_BPC_10 = 10,
	XVIDC_BPC_12 = 12,
	XVIDC_BPC_14 = 14,
	XVIDC_BPC_16 = 16,
	XVIDC_BPC_NUM_SUPPORTED = 6,
	XVIDC_BPC_UNKNOWN
} XVidC_ColorDepth;

/**
 * Pixels per clock.
 */
typedef enum {
	XVIDC_PPC_1 = 1,
	XVIDC_PPC_2 = 2,
	XVIDC_PPC_4 = 4,
	XVIDC_PPC_8 = 8,
	XVIDC_PPC_NUM_SUPPORTED = 4,
} XVidC_PixelsPerClock;

/**
 * Color space format.
 */
typedef enum {
	/* Streaming video formats */
	XVIDC_CSF_RGB = 0,
	XVIDC_CSF_YCRCB_444,
	XVIDC_CSF_YCRCB_422,
	XVIDC_CSF_YCRCB_420,
	XVIDC_CSF_YONLY,
	XVIDC_CSF_RGBA,
	XVIDC_CSF_YCRCBA_444,

	/* 6 empty slots reserved for video formats for future
	 * extension
	 */

	/* Video in memory formats */
	XVIDC_CSF_MEM_RGBX8 = 10,   // [31:0] x:B:G:R 8:8:8:8
	XVIDC_CSF_MEM_YUVX8,        // [31:0] x:V:U:Y 8:8:8:8
	XVIDC_CSF_MEM_YUYV8,        // [31:0] V:Y:U:Y 8:8:8:8
	XVIDC_CSF_MEM_RGBA8,        // [31:0] A:B:G:R 8:8:8:8
	XVIDC_CSF_MEM_YUVA8,        // [31:0] A:V:U:Y 8:8:8:8
	XVIDC_CSF_MEM_RGBX10,       // [31:0] x:B:G:R 2:10:10:10
	XVIDC_CSF_MEM_YUVX10,       // [31:0] x:V:U:Y 2:10:10:10
	XVIDC_CSF_MEM_RGB565,       // [15:0] B:G:R 5:6:5
	XVIDC_CSF_MEM_Y_UV8,        // [15:0] Y:Y 8:8, [15:0] V:U 8:8
	XVIDC_CSF_MEM_Y_UV8_420,    // [15:0] Y:Y 8:8, [15:0] V:U 8:8
	XVIDC_CSF_MEM_RGB8,         // [23:0] B:G:R 8:8:8
	XVIDC_CSF_MEM_YUV8,         // [24:0] V:U:Y 8:8:8
	XVIDC_CSF_MEM_Y_UV10,       // [31:0] x:Y:Y:Y 2:10:10:10 [31:0] x:U:V:U 2:10:10:10
	XVIDC_CSF_MEM_Y_UV10_420,   // [31:0] x:Y:Y:Y 2:10:10:10 [31:0] x:U:V:U 2:10:10:10
	XVIDC_CSF_MEM_Y8,           // [31:0] Y:Y:Y:Y 8:8:8:8
	XVIDC_CSF_MEM_Y10,          // [31:0] x:Y:Y:Y 2:10:10:10
	XVIDC_CSF_MEM_BGRA8,        // [31:0] A:R:G:B 8:8:8:8
	XVIDC_CSF_MEM_BGRX8,        // [31:0] X:R:G:B 8:8:8:8
	XVIDC_CSF_MEM_UYVY8,        // [31:0] Y:V:Y:U 8:8:8:8
	XVIDC_CSF_MEM_BGR8,         // [23:0] R:G:B 8:8:8
	XVIDC_CSF_MEM_RGBX12,       // [39:0] x:R:G:B 4:12:12:12
	XVIDC_CSF_MEM_YUVX12,       // [39:0] x:V:U:Y 4:12:12:12
	XVIDC_CSF_MEM_Y_UV12,       // [23:0] Y:Y 12:12, [23:0] V:U 12:12
	XVIDC_CSF_MEM_Y_UV12_420,   // [23:0] Y:Y 12:12, [23:0] V:U 12:12
	XVIDC_CSF_MEM_Y12,          // [39:0] x:Y2:Y1:Y0 4:12:12:12
	XVIDC_CSF_MEM_RGB16,        // [47:0] R:G:B 16:16:16
	XVIDC_CSF_MEM_YUV16,        // [47:0] V:U:Y 16:16:16
	XVIDC_CSF_MEM_Y_UV16,       // [31:0] Y:Y 16:16, [31:0] V:U 16:16
	XVIDC_CSF_MEM_Y_UV16_420,   // [31:0] Y:Y 16:16, [31:0] V:U 16:16
	XVIDC_CSF_MEM_Y16,          // [47:0] Y2:Y1:Y0 16:16:16
	XVIDC_CSF_MEM_END,          // End of memory formats

	/* Streaming formats with components re-ordered */
	XVIDC_CSF_YCBCR_422 = 64,
	XVIDC_CSF_YCBCR_420,
	XVIDC_CSF_YCBCR_444,

	XVIDC_CSF_NUM_SUPPORTED,    // includes the reserved slots
	XVIDC_CSF_UNKNOWN,
	XVIDC_CSF_STRM_START = XVIDC_CSF_RGB,
	XVIDC_CSF_STRM_END   = XVIDC_CSF_YONLY,
	XVIDC_CSF_MEM_START  = XVIDC_CSF_MEM_RGBX8,
	XVIDC_CSF_NUM_STRM   = (XVIDC_CSF_STRM_END - XVIDC_CSF_STRM_START + 1),
	XVIDC_CSF_NUM_MEM    = (XVIDC_CSF_MEM_END - XVIDC_CSF_MEM_START)
} XVidC_ColorFormat;


/**
 * Image Aspect Ratio.
 */
typedef enum {
	XVIDC_AR_4_3 = 0,
	XVIDC_AR_16_9 = 1
} XVidC_AspectRatio;

/**
 * Color space conversion standard.
 */
typedef enum {
	XVIDC_BT_2020 = 0,
	XVIDC_BT_709,
	XVIDC_BT_601,
	XVIDC_BT_NUM_SUPPORTED,
	XVIDC_BT_UNKNOWN
} XVidC_ColorStd;

/**
 * Color conversion output range.
 */
typedef enum {
	XVIDC_CR_16_235 = 0,
	XVIDC_CR_16_240,
	XVIDC_CR_0_255,
	XVIDC_CR_NUM_SUPPORTED,
	XVIDC_CR_UNKNOWN_RANGE
} XVidC_ColorRange;

/**
 * 3D formats.
 */
typedef enum {
	XVIDC_3D_FRAME_PACKING = 0,	/**< Frame packing.         */
	XVIDC_3D_FIELD_ALTERNATIVE,	/**< Field alternative.     */
	XVIDC_3D_LINE_ALTERNATIVE,	/**< Line alternative.      */
	XVIDC_3D_SIDE_BY_SIDE_FULL,	/**< Side-by-side (full).   */
	XVIDC_3D_TOP_AND_BOTTOM_HALF,	/**< Top-and-bottom (half). */
	XVIDC_3D_SIDE_BY_SIDE_HALF,	/**< Side-by-side (half).   */
	XVIDC_3D_UNKNOWN
} XVidC_3DFormat;

/**
 * 3D Sub-sampling methods.
 */
typedef enum {
	XVIDC_3D_SAMPLING_HORIZONTAL = 0, /**< Horizontal sub-sampling. */
	XVIDC_3D_SAMPLING_QUINCUNX,	  /**< Quincunx matrix.         */
	XVIDC_3D_SAMPLING_UNKNOWN
} XVidC_3DSamplingMethod;

/**
 * 3D Sub-sampling positions.
 */
typedef enum {
	XVIDC_3D_SAMPPOS_OLOR = 0,	/**< Odd/Left,  Odd/Right.  */
	XVIDC_3D_SAMPPOS_OLER,		/**< Odd/Left,  Even/Right. */
	XVIDC_3D_SAMPPOS_ELOR,		/**< Even/Left, Odd/Right.  */
	XVIDC_3D_SAMPPOS_ELER,		/**< Even/Left, Even/Right. */
	XVIDC_3D_SAMPPOS_UNKNOWN
} XVidC_3DSamplingPosition;

/****************************** Type Definitions ******************************/

/**
 * Video timing structure.
 */
typedef struct {
	u16 HActive;
	u16 HFrontPorch;
	u16 HSyncWidth;
	u16 HBackPorch;
	u16 HTotal;
	u8 HSyncPolarity;
	u16 VActive;
	u16 F0PVFrontPorch;
	u16 F0PVSyncWidth;
	u16 F0PVBackPorch;
	u16 F0PVTotal;
	u16 F1VFrontPorch;
	u16 F1VSyncWidth;
	u16 F1VBackPorch;
	u16 F1VTotal;
	u8 VSyncPolarity;
} XVidC_VideoTiming;

/**
 * 3D Sampling info structure.
 */
typedef struct {
	XVidC_3DSamplingMethod   Method;
	XVidC_3DSamplingPosition Position;
} XVidC_3DSamplingInfo;

/**
 * 3D info structure.
 */
typedef struct {
	XVidC_3DFormat		  Format;
	XVidC_3DSamplingInfo  Sampling;
} XVidC_3DInfo;

/**
 * Electro Optical Transfer Function
 *
 * Based on CTA861-G
 */
typedef enum {
	/* TG - Traditional Gamma */
	XVIDC_EOTF_TG_SDR = 0,
	XVIDC_EOTF_TG_HDR,
	XVIDC_EOTF_SMPTE2084,
	XVIDC_EOTF_HLG,
	XVIDC_EOTF_NUM_SUPPORTED,
	XVIDC_EOTF_UNKNOWN,
} XVidC_Eotf;

/**
 * Video stream structure.
 */
typedef struct {
	XVidC_ColorFormat	  ColorFormatId;
	XVidC_ColorDepth	  ColorDepth;
	XVidC_PixelsPerClock  PixPerClk;
	XVidC_FrameRate		  FrameRate;
	XVidC_AspectRatio	  AspectRatio;
	u8			          IsInterlaced;
	u8			          Is3D;
	XVidC_3DInfo		  Info_3D;
	XVidC_VideoMode		  VmId;
	XVidC_VideoTiming	  Timing;
	XVidC_Eotf		Eotf;
	XVidC_ColorStd		ColorStd;
} XVidC_VideoStream;

/**
 * Video window structure.
 */
typedef struct {
	u32 StartX;
	u32 StartY;
	u32 Width;
	u32 Height;
} XVidC_VideoWindow;

/**
 * Video timing mode from the video timing table.
 */
typedef struct {
	XVidC_VideoMode		VmId;
	char			Name[21];
	XVidC_FrameRate		FrameRate;
	XVidC_VideoTiming	Timing;
} XVidC_VideoTimingMode;

/**
 * Callback type which represents a custom timer wait handler. This is only
 * used for Microblaze since it doesn't have a native sleep function. To avoid
 * dependency on a hardware timer, the default wait functionality is implemented
 * using loop iterations; this isn't too accurate. Therefore a custom timer
 * handler is used, the user may implement their own wait implementation.
 *
 * @param	TimerPtr is a pointer to the timer instance.
 * @param	Delay is the duration (msec/usec) to be passed to the timer
 *		function.
 *
*******************************************************************************/
typedef void (*XVidC_DelayHandler)(void *TimerPtr, u32 Delay);

/**************************** Function Prototypes *****************************/

u32 XVidC_RegisterCustomTimingModes(const XVidC_VideoTimingMode *CustomTable,
		                            u16 NumElems);
void XVidC_UnregisterCustomTimingModes(void);
u64 XVidC_GetPixelClockHzByHVFr(u32 HTotal, u32 VTotal, u8 FrameRate);
u64 XVidC_GetPixelClockHzByVmId(XVidC_VideoMode VmId);
XVidC_VideoFormat XVidC_GetVideoFormat(XVidC_VideoMode VmId);
u8 XVidC_IsInterlaced(XVidC_VideoMode VmId);
const XVidC_VideoTimingMode* XVidC_GetVideoModeData(XVidC_VideoMode VmId);
const char *XVidC_GetVideoModeStr(XVidC_VideoMode VmId);
const char *XVidC_GetFrameRateStr(XVidC_VideoMode VmId);
const char *XVidC_GetColorFormatStr(XVidC_ColorFormat ColorFormatId);
XVidC_FrameRate XVidC_GetFrameRate(XVidC_VideoMode VmId);
const XVidC_VideoTiming* XVidC_GetTimingInfo(XVidC_VideoMode VmId);
void XVidC_ReportStreamInfo(const XVidC_VideoStream *Stream);
void XVidC_ReportTiming(const XVidC_VideoTiming *Timing, u8 IsInterlaced);
const char *XVidC_Get3DFormatStr(XVidC_3DFormat Format);
u32 XVidC_SetVideoStream(XVidC_VideoStream *VidStrmPtr, XVidC_VideoMode VmId,
			             XVidC_ColorFormat ColorFormat, XVidC_ColorDepth Bpc,
			             XVidC_PixelsPerClock Ppc);
u32 XVidC_Set3DVideoStream(XVidC_VideoStream *VidStrmPtr, XVidC_VideoMode VmId,
			               XVidC_ColorFormat ColorFormat, XVidC_ColorDepth Bpc,
			               XVidC_PixelsPerClock Ppc, XVidC_3DInfo *Info3DPtr);
XVidC_VideoMode XVidC_GetVideoModeId(u32 Width, u32 Height, u32 FrameRate,
		                             u8 IsInterlaced);
XVidC_VideoMode XVidC_GetVideoModeIdExtensive(XVidC_VideoTiming *Timing,
											  u32 FrameRate,
											  u8 IsInterlaced,
											  u8 IsExtensive);
XVidC_VideoMode XVidC_GetVideoModeIdRb(u32 Width, u32 Height, u32 FrameRate,
		                               u8 IsInterlaced, u8 RbN);
XVidC_VideoMode XVidC_GetVideoModeIdWBlanking(const XVidC_VideoTiming *Timing,
		                                      u32 FrameRate, u8 IsInterlaced);

/******************* Macros (Inline Functions) Definitions ********************/

/*****************************************************************************/
/**
 * This macro check if video stream is 3D or 2D.
 *
 * @param	VidStreamPtr is a pointer to the XVidC_VideoStream structure.
 *
 * @return	3D(1)/2D(0)
 *
 * @note	C-style signature:
 *		u8 XDp_IsStream3D(XVidC_VideoStream *VidStreamPtr)
 *
 *****************************************************************************/
#define XVidC_IsStream3D(VidStreamPtr)       ((VidStreamPtr)->Is3D)

/*************************** Variable Declarations ****************************/

#ifdef __cplusplus
}
#endif

#endif /* XVIDC_H_ */
/** @} */
