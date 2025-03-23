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
// 0x10 : Data signal of pDescLuma
//        bit 31~0 - pDescLuma[31:0] (Read/Write)
// 0x14 : Data signal of pDescLuma
//        bit 31~0 - pDescLuma[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of nDescLumaCount
//        bit 31~0 - nDescLumaCount[31:0] (Read/Write)
// 0x20 : reserved
// 0x24 : Data signal of pDstLuma
//        bit 31~0 - pDstLuma[31:0] (Read/Write)
// 0x28 : Data signal of pDstLuma
//        bit 31~0 - pDstLuma[63:32] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of nDstLumaStride
//        bit 31~0 - nDstLumaStride[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of pDescChroma
//        bit 31~0 - pDescChroma[31:0] (Read/Write)
// 0x3c : Data signal of pDescChroma
//        bit 31~0 - pDescChroma[63:32] (Read/Write)
// 0x40 : reserved
// 0x44 : Data signal of nDescChromaCount
//        bit 31~0 - nDescChromaCount[31:0] (Read/Write)
// 0x48 : reserved
// 0x4c : Data signal of pDstChroma
//        bit 31~0 - pDstChroma[31:0] (Read/Write)
// 0x50 : Data signal of pDstChroma
//        bit 31~0 - pDstChroma[63:32] (Read/Write)
// 0x54 : reserved
// 0x58 : Data signal of nDstChromaStride
//        bit 31~0 - nDstChromaStride[31:0] (Read/Write)
// 0x5c : reserved
// 0x60 : Data signal of nWidth
//        bit 31~0 - nWidth[31:0] (Read/Write)
// 0x64 : reserved
// 0x68 : Data signal of nHeight
//        bit 31~0 - nHeight[31:0] (Read/Write)
// 0x6c : reserved
// 0x70 : Data signal of nFormat
//        bit 31~0 - nFormat[31:0] (Read/Write)
// 0x74 : reserved
// 0x78 : Data signal of nAxisRes
//        bit 31~0 - nAxisRes[31:0] (Read)
// 0x7c : Control signal of nAxisRes
//        bit 0  - nAxisRes_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL               0x00
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_GIE                   0x04
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_IER                   0x08
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_ISR                   0x0c
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCLUMA_DATA        0x10
#define XZ_FRMBUF_WRITER_CONTROL_BITS_PDESCLUMA_DATA        64
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCLUMACOUNT_DATA   0x1c
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NDESCLUMACOUNT_DATA   32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTLUMA_DATA         0x24
#define XZ_FRMBUF_WRITER_CONTROL_BITS_PDSTLUMA_DATA         64
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTLUMASTRIDE_DATA   0x30
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NDSTLUMASTRIDE_DATA   32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCCHROMA_DATA      0x38
#define XZ_FRMBUF_WRITER_CONTROL_BITS_PDESCCHROMA_DATA      64
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCCHROMACOUNT_DATA 0x44
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NDESCCHROMACOUNT_DATA 32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTCHROMA_DATA       0x4c
#define XZ_FRMBUF_WRITER_CONTROL_BITS_PDSTCHROMA_DATA       64
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTCHROMASTRIDE_DATA 0x58
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NDSTCHROMASTRIDE_DATA 32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NWIDTH_DATA           0x60
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NWIDTH_DATA           32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NHEIGHT_DATA          0x68
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NHEIGHT_DATA          32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NFORMAT_DATA          0x70
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NFORMAT_DATA          32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NAXISRES_DATA         0x78
#define XZ_FRMBUF_WRITER_CONTROL_BITS_NAXISRES_DATA         32
#define XZ_FRMBUF_WRITER_CONTROL_ADDR_NAXISRES_CTRL         0x7c

