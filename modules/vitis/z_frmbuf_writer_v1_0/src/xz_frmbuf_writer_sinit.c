// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#ifdef SDT
#include "xparameters.h"
#endif
#include "xz_frmbuf_writer.h"

extern XZ_frmbuf_writer_Config XZ_frmbuf_writer_ConfigTable[];

#ifdef SDT
XZ_frmbuf_writer_Config *XZ_frmbuf_writer_LookupConfig(UINTPTR BaseAddress) {
	XZ_frmbuf_writer_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XZ_frmbuf_writer_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XZ_frmbuf_writer_ConfigTable[Index].Control_BaseAddress == BaseAddress) {
			ConfigPtr = &XZ_frmbuf_writer_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XZ_frmbuf_writer_Initialize(XZ_frmbuf_writer *InstancePtr, UINTPTR BaseAddress) {
	XZ_frmbuf_writer_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XZ_frmbuf_writer_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XZ_frmbuf_writer_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XZ_frmbuf_writer_Config *XZ_frmbuf_writer_LookupConfig(u16 DeviceId) {
	XZ_frmbuf_writer_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XZ_FRMBUF_WRITER_NUM_INSTANCES; Index++) {
		if (XZ_frmbuf_writer_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XZ_frmbuf_writer_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XZ_frmbuf_writer_Initialize(XZ_frmbuf_writer *InstancePtr, u16 DeviceId) {
	XZ_frmbuf_writer_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XZ_frmbuf_writer_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XZ_frmbuf_writer_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

