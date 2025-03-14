#include <stdio.h>

__global__ void kernel_YCbCr422_8u_C2P3R(uchar1* pSrc, int srcStep, uchar1* pDst0, int dst0Step,
	uchar1* pDst1, int dst1Step, uchar1* pDst2, int dst2Step, int nWidth, int nHeight) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if(x < nWidth && y < nHeight) {
		int nSrcIdx = y * srcStep + x * 4;
		int nDst0Idx = y * dst0Step + x * 2;
		int nDst1Idx = y * dst1Step + x;
		int nDst2Idx = y * dst2Step + x;

		// Y1
		pDst0[nDst0Idx + 0] = pSrc[nSrcIdx + 0];
		// Cb
		pDst1[nDst1Idx + 0] = pSrc[nSrcIdx + 1];
		// Y2
		pDst0[nDst0Idx + 1] = pSrc[nSrcIdx + 2];
		// Cr
		pDst2[nDst2Idx + 0] = pSrc[nSrcIdx + 3];
	}
}

extern cudaError_t zppiYCbCr422_8u_C2P3R(uchar1* pSrc, int srcStep, uchar1* pDst[3], int dstStep[3], int nWidth, int nHeight) {
	static int BLOCK_W = 16;
	static int BLOCK_H = 16;

	nWidth /= 2;

	dim3 grid((nWidth + BLOCK_W-1) / BLOCK_W, (nHeight + BLOCK_H-1) / BLOCK_H, 1);
	dim3 block(BLOCK_W, BLOCK_H, 1);

	kernel_YCbCr422_8u_C2P3R<<<grid, block>>>(
		pSrc, srcStep,
		pDst[0], dstStep[0],
		pDst[1], dstStep[1],
		pDst[2], dstStep[2],
		nWidth, nHeight);

	return cudaDeviceSynchronize();
}

__global__ void kernel_CbCr422_CbCr420_8u_P2C2R(uchar1* pSrc0, int src0Step, uchar1* pSrc1, int src1Step,
	uchar1* pDst, int dstStep, int nWidth, int nHeight) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if(x < nWidth && y < nHeight) {
		int nSrc0Idx = y * src0Step + x;
		int nSrc1Idx = y * src1Step + x;
		int nDstIdx = y * dstStep + x * 2;

		// Cb
		pDst[nDstIdx + 0] = pSrc0[nSrc0Idx];
		// Cr
		pDst[nDstIdx + 1] = pSrc1[nSrc1Idx];
	}
}

extern cudaError_t zppiCbCr422_CbCr420_8u_P2C2R(uchar1* pSrc[2], int srcStep[2], uchar1* pDst, int dstStep, int nWidth, int nHeight) {
	static int BLOCK_W = 16;
	static int BLOCK_H = 16;

	dim3 grid((nWidth + BLOCK_W-1) / BLOCK_W, (nHeight + BLOCK_H-1) / BLOCK_H, 1);
	dim3 block(BLOCK_W, BLOCK_H, 1);

	kernel_CbCr422_CbCr420_8u_P2C2R<<<grid, block>>>(
		pSrc[0], srcStep[0],
		pSrc[1], srcStep[1],
		pDst, dstStep,
		nWidth, nHeight);

	return cudaDeviceSynchronize();
}
