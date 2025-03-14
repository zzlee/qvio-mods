#ifndef __ZZ_CUDA_H__
#define __ZZ_CUDA_H__
#if BUILD_WITH_CUDA

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cudaEGL.h>

extern cudaError_t zppiYCbCr422_8u_C2P3R(uchar1* pSrc, int srcStep, uchar1* pDst[3], int dstStep[3], int nWidth, int nHeight);
extern cudaError_t zppiCbCr422_CbCr420_8u_P2C2R(uchar1* pSrc[2], int srcStep[2], uchar1* pDst, int dstStep, int nWidth, int nHeight);

#endif // BUILD_WITH_CUDA
#endif