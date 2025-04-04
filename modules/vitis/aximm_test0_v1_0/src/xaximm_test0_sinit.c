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
#include "xaximm_test0.h"

extern XAximm_test0_Config XAximm_test0_ConfigTable[];

#ifdef SDT
XAximm_test0_Config *XAximm_test0_LookupConfig(UINTPTR BaseAddress) {
	XAximm_test0_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XAximm_test0_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XAximm_test0_ConfigTable[Index].Control_BaseAddress == BaseAddress) {
			ConfigPtr = &XAximm_test0_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XAximm_test0_Initialize(XAximm_test0 *InstancePtr, UINTPTR BaseAddress) {
	XAximm_test0_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XAximm_test0_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XAximm_test0_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XAximm_test0_Config *XAximm_test0_LookupConfig(u16 DeviceId) {
	XAximm_test0_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XAXIMM_TEST0_NUM_INSTANCES; Index++) {
		if (XAximm_test0_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XAximm_test0_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XAximm_test0_Initialize(XAximm_test0 *InstancePtr, u16 DeviceId) {
	XAximm_test0_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XAximm_test0_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XAximm_test0_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

