// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xz_frmbuf_writer.h"

/************************** Function Implementation *************************/
#ifndef __qcap_linux__
int XZ_frmbuf_writer_CfgInitialize(XZ_frmbuf_writer *InstancePtr, XZ_frmbuf_writer_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XZ_frmbuf_writer_Start(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL) & 0x80;
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XZ_frmbuf_writer_IsDone(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XZ_frmbuf_writer_IsIdle(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XZ_frmbuf_writer_IsReady(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XZ_frmbuf_writer_EnableAutoRestart(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XZ_frmbuf_writer_DisableAutoRestart(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_AP_CTRL, 0);
}

void XZ_frmbuf_writer_Set_pDescLuma(XZ_frmbuf_writer *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCLUMA_DATA, (u32)(Data));
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCLUMA_DATA + 4, (u32)(Data >> 32));
}

u64 XZ_frmbuf_writer_Get_pDescLuma(XZ_frmbuf_writer *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCLUMA_DATA);
    Data += (u64)XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCLUMA_DATA + 4) << 32;
    return Data;
}

void XZ_frmbuf_writer_Set_nDescLumaCount(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCLUMACOUNT_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nDescLumaCount(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCLUMACOUNT_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_pDstLuma(XZ_frmbuf_writer *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTLUMA_DATA, (u32)(Data));
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTLUMA_DATA + 4, (u32)(Data >> 32));
}

u64 XZ_frmbuf_writer_Get_pDstLuma(XZ_frmbuf_writer *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTLUMA_DATA);
    Data += (u64)XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTLUMA_DATA + 4) << 32;
    return Data;
}

void XZ_frmbuf_writer_Set_nDstLumaStride(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTLUMASTRIDE_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nDstLumaStride(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTLUMASTRIDE_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_pDescChroma(XZ_frmbuf_writer *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCCHROMA_DATA, (u32)(Data));
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCCHROMA_DATA + 4, (u32)(Data >> 32));
}

u64 XZ_frmbuf_writer_Get_pDescChroma(XZ_frmbuf_writer *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCCHROMA_DATA);
    Data += (u64)XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDESCCHROMA_DATA + 4) << 32;
    return Data;
}

void XZ_frmbuf_writer_Set_nDescChromaCount(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCCHROMACOUNT_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nDescChromaCount(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDESCCHROMACOUNT_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_pDstChroma(XZ_frmbuf_writer *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTCHROMA_DATA, (u32)(Data));
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTCHROMA_DATA + 4, (u32)(Data >> 32));
}

u64 XZ_frmbuf_writer_Get_pDstChroma(XZ_frmbuf_writer *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTCHROMA_DATA);
    Data += (u64)XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_PDSTCHROMA_DATA + 4) << 32;
    return Data;
}

void XZ_frmbuf_writer_Set_nDstChromaStride(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTCHROMASTRIDE_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nDstChromaStride(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NDSTCHROMASTRIDE_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_nWidth(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NWIDTH_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nWidth(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NWIDTH_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_nHeight(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NHEIGHT_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nHeight(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NHEIGHT_DATA);
    return Data;
}

void XZ_frmbuf_writer_Set_nFormat(XZ_frmbuf_writer *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NFORMAT_DATA, Data);
}

u32 XZ_frmbuf_writer_Get_nFormat(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NFORMAT_DATA);
    return Data;
}

u32 XZ_frmbuf_writer_Get_nAxisRes(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NAXISRES_DATA);
    return Data;
}

u32 XZ_frmbuf_writer_Get_nAxisRes_vld(XZ_frmbuf_writer *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_NAXISRES_CTRL);
    return Data & 0x1;
}

void XZ_frmbuf_writer_InterruptGlobalEnable(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_GIE, 1);
}

void XZ_frmbuf_writer_InterruptGlobalDisable(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_GIE, 0);
}

void XZ_frmbuf_writer_InterruptEnable(XZ_frmbuf_writer *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_IER);
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_IER, Register | Mask);
}

void XZ_frmbuf_writer_InterruptDisable(XZ_frmbuf_writer *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_IER);
    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_IER, Register & (~Mask));
}

void XZ_frmbuf_writer_InterruptClear(XZ_frmbuf_writer *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XZ_frmbuf_writer_WriteReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_ISR, Mask);
}

u32 XZ_frmbuf_writer_InterruptGetEnabled(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_IER);
}

u32 XZ_frmbuf_writer_InterruptGetStatus(XZ_frmbuf_writer *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XZ_frmbuf_writer_ReadReg(InstancePtr->Control_BaseAddress, XZ_FRMBUF_WRITER_CONTROL_ADDR_ISR);
}

