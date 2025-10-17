#include "ZzLog.h"
#include "ZzUtils.h"
#include "ZzClock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <vector>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#if BUILD_WITH_NVBUF
#include <nvbufsurface.h>
#endif // BUILD_WITH_NVBUF

#include "qvio-l4t.h"

ZZ_INIT_LOG("10_qdma")

using namespace __zz_clock__;

namespace __10_qdma__ {
	inline uint32_t fourcc(char a, char b, char c, char d) {
		return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
	}

	struct App {
		typedef App self_t;

		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;

		int nFmt;
		int nWidth;
		int nHeight;
		int nStride;
		int nFrameSize;
		int nBuffers;
		int nTimes;
		qvio_buf_type nBufferType;

		std::vector<uint8_t*> pSysBufs;
#if BUILD_WITH_NVBUF
		std::vector<NvBufSurface*> pNVBuf_surfaces;
#endif // BUILD_WITH_NVBUF

		App(int argc, char **argv) : argc(argc), argv(argv) {
		}

		~App() {
		}

		int Run() {
			int err;
			int64_t now = _clk();

			LOGD("%s::%s", typeid(self_t).name(), __FUNCTION__);

			nFmt = fourcc(0, 0, 0, 0); // RGB
			// nFmt = fourcc('Y', '8', '0', '0');
			// nFmt = fourcc('N', 'V', '1', '6');
			nWidth = 4096;
			nHeight = 2160;
			nStride = 0;
			nBuffers = 4;
			nTimes = nHeight;
			nBufferType = QVIO_BUF_TYPE_USERPTR;
			// nBufferType = QVIO_BUF_TYPE_DMABUF;

			switch(1) { case 1:
				ZzUtils::Scoped ZZ_GUARD_NAME([&]() {
					oFreeStack.Flush();
				});

				if(nFmt == fourcc('Y', '8', '0', '0')) {
					nFrameSize = nWidth * nHeight;
				} else if(nFmt == fourcc('N', 'V', '1', '6')) {
					nFrameSize = nWidth * nHeight * 2;
				} else if(nFmt == fourcc(0, 0, 0, 0)) {
					nFrameSize = nWidth * nHeight * 3;
				} else {
					err = -EINVAL;
					LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
					break;
				}

				switch(nBufferType) {
				case QVIO_BUF_TYPE_USERPTR:
					pSysBufs.resize(nBuffers);
					for(int i = 0;i < pSysBufs.size();i++) {
						int size;
						if(nFmt == fourcc('Y', '8', '0', '0')) {
							nStride = (nWidth + 31) / 32 * 32;
							size = nStride * nHeight;
						} else if(nFmt == fourcc('N', 'V', '1', '6')) {
							nStride = (nWidth + 31) / 32 * 32;
							size = nStride * nHeight * 2;
						} else if(nFmt == fourcc(0, 0, 0, 0)) {
							nStride = (nWidth * 3 + 31) / 32 * 32;
							size = nStride * nHeight;
						} else {
							err = -EINVAL;
							LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
							break;
						}

						void *memptr;
						err = posix_memalign(&memptr, 4096, size);
						if (err) {
							LOGE("%s(%d): posix_memalign failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						oFreeStack += [memptr]() {
							free(memptr);
						};

						pSysBufs[i] = (uint8_t*)memptr;
					}
					break;

				case QVIO_BUF_TYPE_DMABUF: {
#if BUILD_WITH_NVBUF
					NvBufSurfaceCreateParams oNVBufParam;
					memset(&oNVBufParam, 0, sizeof(oNVBufParam));
					oNVBufParam.width = nWidth;
					oNVBufParam.height = nHeight;
					oNVBufParam.layout = NVBUF_LAYOUT_PITCH;
					oNVBufParam.memType = NVBUF_MEM_DEFAULT;
					oNVBufParam.gpuId = 0;
					if(nFmt == fourcc('Y', '8', '0', '0')) {
						oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_GRAY8;
						nStride = (nWidth + 31) / 32 * 32;
					} else if(nFmt == fourcc('N', 'V', '1', '6')) {
						oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_NV16;
						nStride = (nWidth + 31) / 32 * 32;
					} else if(nFmt == fourcc(0, 0, 0, 0)) {
						oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_RGB;
						nStride = (nWidth * 3 + 31) / 32 * 32;
					} else {
						err = -EINVAL;
						LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
						break;
					}

					pNVBuf_surfaces.resize(nBuffers);
					for(int i = 0;i < pNVBuf_surfaces.size();i++) {
						err = NvBufSurfaceCreate(&pNVBuf_surfaces[i], 1, &oNVBufParam);
						if(err) {
							LOGE("%s(%d): NvBufSurfaceCreate() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}

						NvBufSurfaceParams& surfaceParams = pNVBuf_surfaces[i]->surfaceList[0];

						LOGD("surfaceParams={.width=%d, .height=%d, .bufferDesc=%d, .pitch=%d(%d/%d), offset=%d/%d, psize=%d/%d}",
							surfaceParams.width, surfaceParams.height, (int)surfaceParams.bufferDesc,
							surfaceParams.pitch, surfaceParams.planeParams.pitch[0], surfaceParams.planeParams.pitch[1],
							surfaceParams.planeParams.offset[0], surfaceParams.planeParams.offset[1],
							surfaceParams.planeParams.psize[0], surfaceParams.planeParams.psize[1]);

						// fd = (int)surfaceParams.bufferDesc;
						oFreeStack += [&, i]() {
							int err;

							err = NvBufSurfaceDestroy(pNVBuf_surfaces[i]);
							if(err) {
								LOGE("%s(%d): NvBufSurfaceDestroy() failed, err=%d", __FUNCTION__, __LINE__, err);
							}
						};
					}
#endif // BUILD_WITH_NVBUF
				}
					break;

				default:
					err = -EINVAL;
					LOGE("%s(%d): unexpected value, nBufferType=%d", __FUNCTION__, __LINE__, nBufferType);
					break;
				}

				if(err < 0)
					break;

				ZzUtils::TestLoop([&](int ch) -> int {
					int err = 0;

					switch(1) { case 1:
						int64_t now = _clk();

						switch(ch) {
						case '1':
							OnTest1(now, "/dev/qdma_wr0", QVIO_BUF_DIR_FROM_DEVICE);
							// OnTest1(now, "/dev/xdma_wr0", QVIO_BUF_DIR_FROM_DEVICE);
							break;

						case '2':
							OnTest1(now, "/dev/qdma_rd0", QVIO_BUF_DIR_TO_DEVICE);
							// OnTest1(now, "/dev/xdma_rd0", QVIO_BUF_DIR_TO_DEVICE);
							break;

						case '3':
							OnTest1(now, "/dev/qdma_wr1", QVIO_BUF_DIR_FROM_DEVICE);
							break;

						case '4':
							OnTest1(now, "/dev/qdma_wr2", QVIO_BUF_DIR_FROM_DEVICE);
							break;
						}
					}

					return err;
				});
			}

			return err;
		}

		int ReqBufs_sysbuf(int fd) {
			int err;

			switch(1) { case 1:
				qvio_req_bufs args;

				memset(&args, 0, sizeof(args));
				args.count = nBuffers;
				args.buf_type = QVIO_BUF_TYPE_USERPTR;

				if(nFmt == fourcc('Y', '8', '0', '0') || nFmt == fourcc(0, 0, 0, 0)) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
				} else if(nFmt == fourcc('N', 'V', '1', '6')) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
					args.offset[1] = nStride * nHeight;
					args.stride[1] = nStride;
				} else {
					err = EINVAL;
					LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
					break;
				}

				err = ioctl(fd, QVIO_IOC_REQ_BUFS, &args);
				if(err) {
					LOGE("%s(%d): ioctl(QVIO_IOC_REQ_BUFS) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		int EnqueueBuffer_sysbuf(int fd, int nIndex, qvio_buf_dir dir) {
			int err;

			switch(1) { case 1:
				qvio_buffer args;

				memset(&args, 0, sizeof(args));
				args.index = nIndex;
				args.buf_type = QVIO_BUF_TYPE_USERPTR;
				args.buf_dir = dir;
				args.u.userptr = (unsigned long)pSysBufs[nIndex];

				if(nFmt == fourcc('Y', '8', '0', '0') || nFmt == fourcc(0, 0, 0, 0)) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
				} else if(nFmt == fourcc('N', 'V', '1', '6')) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
					args.offset[1] = nStride * nHeight;
					args.stride[1] = nStride;
				} else {
					err = EINVAL;
					LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
					break;
				}

				err = ioctl(fd, QVIO_IOC_QBUF, &args);
				if(err) {
					LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		int ReqBufs_nvbuf(int fd) {
			int err;

			switch(1) { case 1:
				qvio_req_bufs args;

				memset(&args, 0, sizeof(args));
				args.count = nBuffers;
				args.buf_type = QVIO_BUF_TYPE_DMABUF;

				if(nFmt == fourcc('Y', '8', '0', '0') || nFmt == fourcc(0, 0, 0, 0)) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
				} else if(nFmt == fourcc('N', 'V', '1', '6')) {
					args.offset[0] = 0;
					args.stride[0] = nStride;
					args.offset[1] = nStride * nHeight;
					args.stride[1] = nStride;
				} else {
					err = EINVAL;
					LOGE("%s(%d): unexpected value, nFmt=0x%08X", __FUNCTION__, __LINE__, nFmt);
					break;
				}

				err = ioctl(fd, QVIO_IOC_REQ_BUFS, &args);
				if(err) {
					LOGE("%s(%d): ioctl(QVIO_IOC_REQ_BUFS) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		int EnqueueBuffer_nvbuf(int fd, int nIndex, qvio_buf_dir dir) {
			int err;

			switch(1) { case 1:
				NvBufSurfaceParams& surfaceParams = pNVBuf_surfaces[nIndex]->surfaceList[0];

				qvio_buffer args;
				memset(&args, 0, sizeof(args));
				args.index = nIndex;
				args.buf_type = QVIO_BUF_TYPE_DMABUF;
				args.buf_dir = dir;
				args.u.fd = (int)surfaceParams.bufferDesc;
				args.offset[0] = surfaceParams.planeParams.offset[0];
				args.stride[0] = surfaceParams.planeParams.pitch[0];
				args.offset[1] = surfaceParams.planeParams.offset[1];
				args.stride[1] = surfaceParams.planeParams.pitch[1];
				err = ioctl(fd, QVIO_IOC_QBUF, &args);
				if(err) {
					LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		void OnTest1(int64_t now, const char* dev_name, qvio_buf_dir dir) {
			int err;

			switch(1) { case 1:
				int fd_qvio = open(dev_name, O_RDWR);
				if(fd_qvio == -1) {
					err = errno;
					LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
				LOGD("open(\"%s\")=%d...\n", dev_name, fd_qvio);
				ZzUtils::Scoped ZZ_GUARD_NAME([fd_qvio, dev_name]() {
					LOGD("close(\"%s\")=%d...\n", dev_name, fd_qvio);
					close(fd_qvio);
				});

				{
					qvio_format args;
					memset(&args, 0, sizeof(args));
					LOGD("nFmt=%04X", nFmt);
					args.fmt = nFmt;
					args.width = nWidth;
					args.height = nHeight;
					err = ioctl(fd_qvio, QVIO_IOC_S_FMT, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}

				LOGD("QVIO_IOC_REQ_BUFS...");
				{
					switch(nBufferType) {
					case QVIO_BUF_TYPE_USERPTR:
						err = ReqBufs_sysbuf(fd_qvio);
						break;

					case QVIO_BUF_TYPE_DMABUF:
						err = ReqBufs_nvbuf(fd_qvio);
						break;

					default:
						err = -EINVAL;
						LOGE("%s(%d): unexpected value, nBufferType=%d", __FUNCTION__, __LINE__, nBufferType);
						break;
					};
				}
				if(err)
					break;

				LOGD("QVIO_IOC_QBUF...");
				int nQbufs = 0;
				{
					for(int i = 0;i < nBuffers;i++) {
						switch(nBufferType) {
						case QVIO_BUF_TYPE_USERPTR:
							err = EnqueueBuffer_sysbuf(fd_qvio, i, dir);
							break;

						case QVIO_BUF_TYPE_DMABUF:
							err = EnqueueBuffer_nvbuf(fd_qvio, i, dir);
							break;

						default:
							err = -EINVAL;
							LOGE("%s(%d): unexpected value, nBufferType=%d", __FUNCTION__, __LINE__, nBufferType);
							break;
						}

						if(err < 0) {
							LOGE("%s(%d): EnqueueBuffer() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;
					}
				}
				if(err)
					break;

				ZzUtils::ZzStatBitRate oStatBitRate;

				switch(nBufferType) {
				case QVIO_BUF_TYPE_USERPTR:
					oStatBitRate.log_prefix = "userptr";
					break;

				case QVIO_BUF_TYPE_DMABUF:
					oStatBitRate.log_prefix = "dmabuf";
					break;

				default:
					oStatBitRate.log_prefix = "unknown";
					break;
				}

				oStatBitRate.Reset();

				LOGD("QVIO_IOC_STREAMON...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMON);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

#if 1
				int fd_stdin = 0; // stdin
				while(true) {
					fd_set readfds;
					FD_ZERO(&readfds);

					int fd_max = -1;
					if(fd_stdin > fd_max) fd_max = fd_stdin;
					FD_SET(fd_stdin, &readfds);
					if(fd_qvio > fd_max) fd_max = fd_qvio;
					FD_SET(fd_qvio, &readfds);

					err = select(fd_max + 1, &readfds, NULL, NULL, NULL);
					if (err < 0) {
						LOGE("%s(%d): select() failed! err=%d", __FUNCTION__, __LINE__, err);
						break;
					}

					if (FD_ISSET(fd_stdin, &readfds)) {
						int ch = getchar();

						if(ch == 'q')
							break;
					}

					if (FD_ISSET(fd_qvio, &readfds)) {
						int nBufIdx;
						{
							qvio_buffer args;

							err = ioctl(fd_qvio, QVIO_IOC_DQBUF, &args);
							if(err) {
								err = errno;
								LOGE("%s(%d): ioctl(QVIO_IOC_DQBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
								break;
							}
							now = _clk();
							nQbufs--;

							nBufIdx = args.index;
						}

						oStatBitRate.Log(nFrameSize * 8, now);

						// LOGD("QVIO_IOC_QBUF, nBufIdx=%d", nBufIdx);
#if 1
						switch(nBufferType) {
						case QVIO_BUF_TYPE_USERPTR:
							err = EnqueueBuffer_sysbuf(fd_qvio, nBufIdx, dir);
							break;

						case QVIO_BUF_TYPE_DMABUF:
							err = EnqueueBuffer_nvbuf(fd_qvio, nBufIdx, dir);
							break;

						default:
							err = -1;
							errno = EINVAL;
							LOGE("%s(%d): unexpected value, nBufferType=%d", __FUNCTION__, __LINE__, nBufferType);
							break;
						}

						if(err) {
							err = errno;
							LOGE("%s(%d): EnqueueBuffer() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;
#else
						break;
#endif
					}
				}
#else
				LOGD("sleep ...");
				sleep(5);
#endif

				LOGD("QVIO_IOC_STREAMOFF...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMOFF);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}
	};
}

using namespace __10_qdma__;

int main(int argc, char *argv[]) {
	LOGD("entering...");

	int err;
	{
		App app(argc, argv);
		err = app.Run();

		LOGD("leaving...");
	}

	return err;
}
