// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XZ_FRMBUF_WRITER_H
#define XZ_FRMBUF_WRITER_H

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
#include "xz_frmbuf_writer_hw.h"

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
} XZ_frmbuf_writer_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XZ_frmbuf_writer;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __qcap_linux__
#define XZ_frmbuf_writer_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XZ_frmbuf_writer_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XZ_frmbuf_writer_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XZ_frmbuf_writer_ReadReg(BaseAddress, RegOffset) \
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
int XZ_frmbuf_writer_Initialize(XZ_frmbuf_writer *InstancePtr, UINTPTR BaseAddress);
XZ_frmbuf_writer_Config* XZ_frmbuf_writer_LookupConfig(UINTPTR BaseAddress);
#else
int XZ_frmbuf_writer_Initialize(XZ_frmbuf_writer *InstancePtr, u16 DeviceId);
XZ_frmbuf_writer_Config* XZ_frmbuf_writer_LookupConfig(u16 DeviceId);
#endif
int XZ_frmbuf_writer_CfgInitialize(XZ_frmbuf_writer *InstancePtr, XZ_frmbuf_writer_Config *ConfigPtr);
#else
int XZ_frmbuf_writer_Initialize(XZ_frmbuf_writer *InstancePtr, const char* InstanceName);
int XZ_frmbuf_writer_Release(XZ_frmbuf_writer *InstancePtr);
#endif

void XZ_frmbuf_writer_Start(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_IsDone(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_IsIdle(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_IsReady(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_EnableAutoRestart(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_DisableAutoRestart(XZ_frmbuf_writer *InstancePtr);

void XZ_frmbuf_writer_Set_pDescLuma(XZ_frmbuf_writer *InstancePtr, u64 Data);
u64 XZ_frmbuf_writer_Get_pDescLuma(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nDescLumaCount(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nDescLumaCount(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_pDstLuma(XZ_frmbuf_writer *InstancePtr, u64 Data);
u64 XZ_frmbuf_writer_Get_pDstLuma(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nDstLumaStride(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nDstLumaStride(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_pDescChroma(XZ_frmbuf_writer *InstancePtr, u64 Data);
u64 XZ_frmbuf_writer_Get_pDescChroma(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nDescChromaCount(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nDescChromaCount(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_pDstChroma(XZ_frmbuf_writer *InstancePtr, u64 Data);
u64 XZ_frmbuf_writer_Get_pDstChroma(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nDstChromaStride(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nDstChromaStride(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nWidth(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nWidth(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nHeight(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nHeight(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_Set_nFormat(XZ_frmbuf_writer *InstancePtr, u32 Data);
u32 XZ_frmbuf_writer_Get_nFormat(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_Get_nAxisRes(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_Get_nAxisRes_vld(XZ_frmbuf_writer *InstancePtr);

void XZ_frmbuf_writer_InterruptGlobalEnable(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_InterruptGlobalDisable(XZ_frmbuf_writer *InstancePtr);
void XZ_frmbuf_writer_InterruptEnable(XZ_frmbuf_writer *InstancePtr, u32 Mask);
void XZ_frmbuf_writer_InterruptDisable(XZ_frmbuf_writer *InstancePtr, u32 Mask);
void XZ_frmbuf_writer_InterruptClear(XZ_frmbuf_writer *InstancePtr, u32 Mask);
u32 XZ_frmbuf_writer_InterruptGetEnabled(XZ_frmbuf_writer *InstancePtr);
u32 XZ_frmbuf_writer_InterruptGetStatus(XZ_frmbuf_writer *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
