// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XAXIMM_TEST0_H
#define XAXIMM_TEST0_H

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
#include "xaximm_test0_hw.h"

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
} XAximm_test0_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XAximm_test0;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __qcap_linux__
#define XAximm_test0_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XAximm_test0_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XAximm_test0_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XAximm_test0_ReadReg(BaseAddress, RegOffset) \
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
int XAximm_test0_Initialize(XAximm_test0 *InstancePtr, UINTPTR BaseAddress);
XAximm_test0_Config* XAximm_test0_LookupConfig(UINTPTR BaseAddress);
#else
int XAximm_test0_Initialize(XAximm_test0 *InstancePtr, u16 DeviceId);
XAximm_test0_Config* XAximm_test0_LookupConfig(u16 DeviceId);
#endif
int XAximm_test0_CfgInitialize(XAximm_test0 *InstancePtr, XAximm_test0_Config *ConfigPtr);
#else
int XAximm_test0_Initialize(XAximm_test0 *InstancePtr, const char* InstanceName);
int XAximm_test0_Release(XAximm_test0 *InstancePtr);
#endif

void XAximm_test0_Start(XAximm_test0 *InstancePtr);
u32 XAximm_test0_IsDone(XAximm_test0 *InstancePtr);
u32 XAximm_test0_IsIdle(XAximm_test0 *InstancePtr);
u32 XAximm_test0_IsReady(XAximm_test0 *InstancePtr);
void XAximm_test0_EnableAutoRestart(XAximm_test0 *InstancePtr);
void XAximm_test0_DisableAutoRestart(XAximm_test0 *InstancePtr);

void XAximm_test0_Set_pDstPxl(XAximm_test0 *InstancePtr, u64 Data);
u64 XAximm_test0_Get_pDstPxl(XAximm_test0 *InstancePtr);
void XAximm_test0_Set_nSize(XAximm_test0 *InstancePtr, u32 Data);
u32 XAximm_test0_Get_nSize(XAximm_test0 *InstancePtr);
void XAximm_test0_Set_nTimes(XAximm_test0 *InstancePtr, u32 Data);
u32 XAximm_test0_Get_nTimes(XAximm_test0 *InstancePtr);

void XAximm_test0_InterruptGlobalEnable(XAximm_test0 *InstancePtr);
void XAximm_test0_InterruptGlobalDisable(XAximm_test0 *InstancePtr);
void XAximm_test0_InterruptEnable(XAximm_test0 *InstancePtr, u32 Mask);
void XAximm_test0_InterruptDisable(XAximm_test0 *InstancePtr, u32 Mask);
void XAximm_test0_InterruptClear(XAximm_test0 *InstancePtr, u32 Mask);
u32 XAximm_test0_InterruptGetEnabled(XAximm_test0 *InstancePtr);
u32 XAximm_test0_InterruptGetStatus(XAximm_test0 *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
