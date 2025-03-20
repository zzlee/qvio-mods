// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0 - ap_done (Read/TOW)
//        bit 1 - ap_ready (Read/TOW)
//        others - reserved
// 0x10 : Data signal of pDescItem
//        bit 31~0 - pDescItem[31:0] (Read/Write)
// 0x14 : Data signal of pDescItem
//        bit 31~0 - pDescItem[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of nDescItemCount
//        bit 31~0 - nDescItemCount[31:0] (Read/Write)
// 0x20 : reserved
// 0x24 : Data signal of pDstPxl
//        bit 31~0 - pDstPxl[31:0] (Read/Write)
// 0x28 : Data signal of pDstPxl
//        bit 31~0 - pDstPxl[63:32] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of nDstPxlStride
//        bit 31~0 - nDstPxlStride[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of nWidth
//        bit 31~0 - nWidth[31:0] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of nHeight
//        bit 31~0 - nHeight[31:0] (Read/Write)
// 0x44 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XAXIMM_TEST1_CONTROL_ADDR_AP_CTRL             0x00
#define XAXIMM_TEST1_CONTROL_ADDR_GIE                 0x04
#define XAXIMM_TEST1_CONTROL_ADDR_IER                 0x08
#define XAXIMM_TEST1_CONTROL_ADDR_ISR                 0x0c
#define XAXIMM_TEST1_CONTROL_ADDR_PDESCITEM_DATA      0x10
#define XAXIMM_TEST1_CONTROL_BITS_PDESCITEM_DATA      64
#define XAXIMM_TEST1_CONTROL_ADDR_NDESCITEMCOUNT_DATA 0x1c
#define XAXIMM_TEST1_CONTROL_BITS_NDESCITEMCOUNT_DATA 32
#define XAXIMM_TEST1_CONTROL_ADDR_PDSTPXL_DATA        0x24
#define XAXIMM_TEST1_CONTROL_BITS_PDSTPXL_DATA        64
#define XAXIMM_TEST1_CONTROL_ADDR_NDSTPXLSTRIDE_DATA  0x30
#define XAXIMM_TEST1_CONTROL_BITS_NDSTPXLSTRIDE_DATA  32
#define XAXIMM_TEST1_CONTROL_ADDR_NWIDTH_DATA         0x38
#define XAXIMM_TEST1_CONTROL_BITS_NWIDTH_DATA         32
#define XAXIMM_TEST1_CONTROL_ADDR_NHEIGHT_DATA        0x40
#define XAXIMM_TEST1_CONTROL_BITS_NHEIGHT_DATA        32

