// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xaximm_test0.h"

/************************** Function Implementation *************************/
#ifndef __qcap_linux__
int XAximm_test0_CfgInitialize(XAximm_test0 *InstancePtr, XAximm_test0_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XAximm_test0_Start(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL) & 0x80;
    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XAximm_test0_IsDone(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XAximm_test0_IsIdle(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XAximm_test0_IsReady(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XAximm_test0_EnableAutoRestart(XAximm_test0 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XAximm_test0_DisableAutoRestart(XAximm_test0 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_AP_CTRL, 0);
}

void XAximm_test0_Set_pDstPxl(XAximm_test0 *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_PDSTPXL_DATA, (u32)(Data));
    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_PDSTPXL_DATA + 4, (u32)(Data >> 32));
}

u64 XAximm_test0_Get_pDstPxl(XAximm_test0 *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_PDSTPXL_DATA);
    Data += (u64)XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_PDSTPXL_DATA + 4) << 32;
    return Data;
}

void XAximm_test0_Set_nSize(XAximm_test0 *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_NSIZE_DATA, Data);
}

u32 XAximm_test0_Get_nSize(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_NSIZE_DATA);
    return Data;
}

void XAximm_test0_Set_nTimes(XAximm_test0 *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_NTIMES_DATA, Data);
}

u32 XAximm_test0_Get_nTimes(XAximm_test0 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_NTIMES_DATA);
    return Data;
}

void XAximm_test0_InterruptGlobalEnable(XAximm_test0 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_GIE, 1);
}

void XAximm_test0_InterruptGlobalDisable(XAximm_test0 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_GIE, 0);
}

void XAximm_test0_InterruptEnable(XAximm_test0 *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_IER);
    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_IER, Register | Mask);
}

void XAximm_test0_InterruptDisable(XAximm_test0 *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_IER);
    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_IER, Register & (~Mask));
}

void XAximm_test0_InterruptClear(XAximm_test0 *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XAximm_test0_WriteReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_ISR, Mask);
}

u32 XAximm_test0_InterruptGetEnabled(XAximm_test0 *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_IER);
}

u32 XAximm_test0_InterruptGetStatus(XAximm_test0 *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XAximm_test0_ReadReg(InstancePtr->Control_BaseAddress, XAXIMM_TEST0_CONTROL_ADDR_ISR);
}

