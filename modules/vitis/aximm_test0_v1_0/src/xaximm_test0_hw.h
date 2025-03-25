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
// 0x10 : Data signal of pDstPxl
//        bit 31~0 - pDstPxl[31:0] (Read/Write)
// 0x14 : Data signal of pDstPxl
//        bit 31~0 - pDstPxl[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of nSize
//        bit 31~0 - nSize[31:0] (Read/Write)
// 0x20 : reserved
// 0x24 : Data signal of nTimes
//        bit 31~0 - nTimes[31:0] (Read/Write)
// 0x28 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL      0x00
#define XAXIMM_TEST0_CONTROL_ADDR_GIE          0x04
#define XAXIMM_TEST0_CONTROL_ADDR_IER          0x08
#define XAXIMM_TEST0_CONTROL_ADDR_ISR          0x0c
#define XAXIMM_TEST0_CONTROL_ADDR_PDSTPXL_DATA 0x10
#define XAXIMM_TEST0_CONTROL_BITS_PDSTPXL_DATA 64
#define XAXIMM_TEST0_CONTROL_ADDR_NSIZE_DATA   0x1c
#define XAXIMM_TEST0_CONTROL_BITS_NSIZE_DATA   32
#define XAXIMM_TEST0_CONTROL_ADDR_NTIMES_DATA  0x24
#define XAXIMM_TEST0_CONTROL_BITS_NTIMES_DATA  32

