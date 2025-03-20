// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XAXIMM_TEST1_H
#define XAXIMM_TEST1_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __qcap_linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xaximm_test1_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __qcap_linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
#ifdef SDT
    char *Name;
#else
    u16 DeviceId;
#endif
    u64 Control_BaseAddress;
} XAximm_test1_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XAximm_test1;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __qcap_linux__
#define XAximm_test1_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XAximm_test1_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XAximm_test1_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XAximm_test1_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __qcap_linux__
#ifdef SDT
int XAximm_test1_Initialize(XAximm_test1 *InstancePtr, UINTPTR BaseAddress);
XAximm_test1_Config* XAximm_test1_LookupConfig(UINTPTR BaseAddress);
#else
int XAximm_test1_Initialize(XAximm_test1 *InstancePtr, u16 DeviceId);
XAximm_test1_Config* XAximm_test1_LookupConfig(u16 DeviceId);
#endif
int XAximm_test1_CfgInitialize(XAximm_test1 *InstancePtr, XAximm_test1_Config *ConfigPtr);
#else
int XAximm_test1_Initialize(XAximm_test1 *InstancePtr, const char* InstanceName);
int XAximm_test1_Release(XAximm_test1 *InstancePtr);
#endif

void XAximm_test1_Start(XAximm_test1 *InstancePtr);
u32 XAximm_test1_IsDone(XAximm_test1 *InstancePtr);
u32 XAximm_test1_IsIdle(XAximm_test1 *InstancePtr);
u32 XAximm_test1_IsReady(XAximm_test1 *InstancePtr);
void XAximm_test1_EnableAutoRestart(XAximm_test1 *InstancePtr);
void XAximm_test1_DisableAutoRestart(XAximm_test1 *InstancePtr);

void XAximm_test1_Set_pDescItem(XAximm_test1 *InstancePtr, u64 Data);
u64 XAximm_test1_Get_pDescItem(XAximm_test1 *InstancePtr);
void XAximm_test1_Set_nDescItemCount(XAximm_test1 *InstancePtr, u32 Data);
u32 XAximm_test1_Get_nDescItemCount(XAximm_test1 *InstancePtr);
void XAximm_test1_Set_pDstPxl(XAximm_test1 *InstancePtr, u64 Data);
u64 XAximm_test1_Get_pDstPxl(XAximm_test1 *InstancePtr);
void XAximm_test1_Set_nDstPxlStride(XAximm_test1 *InstancePtr, u32 Data);
u32 XAximm_test1_Get_nDstPxlStride(XAximm_test1 *InstancePtr);
void XAximm_test1_Set_nWidth(XAximm_test1 *InstancePtr, u32 Data);
u32 XAximm_test1_Get_nWidth(XAximm_test1 *InstancePtr);
void XAximm_test1_Set_nHeight(XAximm_test1 *InstancePtr, u32 Data);
u32 XAximm_test1_Get_nHeight(XAximm_test1 *InstancePtr);

void XAximm_test1_InterruptGlobalEnable(XAximm_test1 *InstancePtr);
void XAximm_test1_InterruptGlobalDisable(XAximm_test1 *InstancePtr);
void XAximm_test1_InterruptEnable(XAximm_test1 *InstancePtr, u32 Mask);
void XAximm_test1_InterruptDisable(XAximm_test1 *InstancePtr, u32 Mask);
void XAximm_test1_InterruptClear(XAximm_test1 *InstancePtr, u32 Mask);
u32 XAximm_test1_InterruptGetEnabled(XAximm_test1 *InstancePtr);
u32 XAximm_test1_InterruptGetStatus(XAximm_test1 *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
