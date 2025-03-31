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

#include "qvio-l4t.h"

#if BUILD_WITH_NPP
#include <npp.h>
#include <nppi.h>
#include <npps.h>
#endif // BUILD_WITH_NPP

#if BUILD_WITH_NVBUF
#include <nvbufsurface.h>
#endif // BUILD_WITH_NVBUF

#define CONCAT_I(N, S) N ## S
#define CONCAT(N, S) CONCAT_I(N, S)
#define GUARD_NAME CONCAT(__GUARD_, __LINE__)

ZZ_INIT_LOG("06_aximm-test1")

using namespace __zz_clock__;

namespace __06_aximm_test1__ {
	inline uint32_t fourcc(char a, char b, char c, char d) {
		return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
	}

	struct App {
		typedef App self_t;

		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;

		int nWidth;
		int nHeight;
		int nStride;
		int nBuffers;

		int fd_qvio;
		bool bMeasureTicks;
		ZzUtils::RateCtrl oRateCtrl;
		int64_t next_time;
		int64_t last_tick_time;
		__u32 last_ticks;

		std::vector<qvio_buffer> pMmapBufs;
#if BUILD_WITH_NVBUF
		std::vector<NvBufSurface*> pNVBuf_surfaces;
#endif // BUILD_WITH_NVBUF
		std::vector<uint8_t*> pSysBufs;

		App(int argc, char **argv) : argc(argc), argv(argv) {
		}

		~App() {
		}

		int Run() {
			int err;
			int64_t now = _clk();

			LOGD("%s::%s", typeid(self_t).name(), __FUNCTION__);

			nWidth = 4096;
			nHeight = 2160 * 2;
			nStride = 4096;
			nBuffers = 4;

			bMeasureTicks = false;
			oRateCtrl.num = 2 * 1000LL;
			oRateCtrl.den = 1000LL;

			switch(1) { case 1:
				std::shared_ptr<void> GUARD_NAME(NULL, [&](void*) {
					oFreeStack.Flush();
				});

				fd_qvio = open("/dev/qvio0", O_RDWR);
				if(fd_qvio == -1) {
					err = errno;
					LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
				oFreeStack += [&]() {
					close(fd_qvio);
				};

				{
					int work_mode = QVIO_WORK_MODE_AXIMM_TEST1;
					err = ioctl(fd_qvio, QVIO_IOC_S_WORK_MODE, &work_mode);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_S_WORK_MODE) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}

				{
					qvio_format args;
					memset(&args, 0, sizeof(args));
					args.width = nWidth;
					args.height = nHeight;
					args.fmt = fourcc('Y', '8', '0', '0');
					err = ioctl(fd_qvio, QVIO_IOC_S_FMT, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}

				{
					qvio_req_bufs args;
					memset(&args, 0, sizeof(args));
					args.count = nBuffers;
					args.buf_type = QVIO_BUF_TYPE_MMAP;
					args.offset[0] = 0;
					args.stride[0] = nStride;
					err = ioctl(fd_qvio, QVIO_IOC_REQ_BUFS, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_REQ_BUFS) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}

					if(args.count != nBuffers) {
						LOGE("%s(%d): unexpected value, (%d != %d)", args.count, nBuffers);
						break;
					}

					LOGD("args.count=%d", args.count);
				}

				pMmapBufs.resize(nBuffers);
				for(int i = 0;i < pMmapBufs.size();i++) {
					qvio_buffer args;
					memset(&args, 0, sizeof(args));
					args.index = i;
					err = ioctl(fd_qvio, QVIO_IOC_QUERY_BUF, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_QUERY_BUF) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}


					LOGD("args={ .index=%d, .buf_type=%d, .u.offset=%d, .offset[0]=%d, .stride[0]=%d }",
						(int)args.index, (int)args.buf_type, (int)args.u.offset, (int)args.offset[0], (int)args.stride[0]);

					pMmapBufs[i] = args;
				}

#if BUILD_WITH_NVBUF
				NvBufSurfaceCreateParams oNVBufParam;
				memset(&oNVBufParam, 0, sizeof(oNVBufParam));
				oNVBufParam.width = nWidth;
				oNVBufParam.height = nHeight;
				oNVBufParam.layout = NVBUF_LAYOUT_PITCH;
				oNVBufParam.memType = NVBUF_MEM_DEFAULT;
				oNVBufParam.gpuId = 0;
				oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_GRAY8;

				pNVBuf_surfaces.resize(nBuffers);
				for(int i = 0;i < pNVBuf_surfaces.size();i++) {
					err = NvBufSurfaceCreate(&pNVBuf_surfaces[i], 1, &oNVBufParam);
					if(err) {
						err = errno;
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
							err = errno;
							LOGE("%s(%d): NvBufSurfaceDestroy() failed, err=%d", __FUNCTION__, __LINE__, err);
						}
					};
				}
				if(err < 0)
					break;
#endif // BUILD_WITH_NVBUF

				pSysBufs.resize(nBuffers);
				for(int i = 0;i < pSysBufs.size();i++) {
					void *memptr;
					err = posix_memalign(&memptr, 4096, nStride * nHeight);
					if (err) {
						LOGE("%s(%d): posix_memalign failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					oFreeStack += [memptr]() {
						free(memptr);
					};

					pSysBufs[i] = (uint8_t*)memptr;
				}
				if(err < 0)
					break;

				ZzUtils::TestLoop([&](int ch) -> int {
					int err = 0;

					switch(1) { case 1:
						int64_t now = _clk();

						OnTickMasure(now);

						switch(ch) {
						case 't':
						case 'T':
							if(bMeasureTicks) {
								bMeasureTicks = false;
							} else {
								last_ticks = 0;
								last_tick_time = now;
								next_time = now;
								oRateCtrl.Start(now);
								bMeasureTicks = true;
							}
							break;

						case 's':
						case 'S':
							OnSnapshot(now);
							break;

						case '1':
							OnTest1(now);
							break;

						case '2':
							OnTest2(now);
							break;

						case '3':
							OnTest3(now);
							break;

						case '4':
							OnTest4(now);
							break;

						case '5':
							OnTest5(now);
							break;

						case '6':
							OnTest6(now);
							break;
						}
					}

					return err;
				});
			}

			return err;
		}

		void OnTickMasure(int64_t now) {
			int err;

			switch(1) { case 1:
				if(! bMeasureTicks)
					break;

				if(! oRateCtrl.Next(now))
					break;

				next_time += oRateCtrl.Advance();

				qvio_g_ticks args;
				err = ioctl(fd_qvio, QVIO_IOC_G_TICKS, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_G_TICKS) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("%.2f", 1.0 * (args.ticks - last_ticks) / (now - last_tick_time));

				last_ticks = args.ticks;
				last_tick_time = now;
			}
		}

		void OnSnapshot(int64_t now) {
		}

		int EnqueueBuffer_sysbuf(int nIndex) {
			int err;

			switch(1) { case 1:
				qvio_buffer args;

				memset(&args, 0, sizeof(args));
				args.index = nIndex;
				args.buf_type = QVIO_BUF_TYPE_USERPTR;
				args.u.userptr = (unsigned long)pSysBufs[nIndex];
				args.offset[0] = 0;
				args.stride[0] = nStride;
				err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		int EnqueueBuffer_nvbuf(int nIndex) {
			int err;

			switch(1) { case 1:
				NvBufSurfaceParams& surfaceParams = pNVBuf_surfaces[nIndex]->surfaceList[0];

				qvio_buffer args;
				memset(&args, 0, sizeof(args));
				args.index = nIndex;
				args.buf_type = QVIO_BUF_TYPE_DMABUF;
				args.u.fd = (int)surfaceParams.bufferDesc;
				args.offset[0] = surfaceParams.planeParams.offset[0];
				args.stride[0] = surfaceParams.planeParams.pitch[0];
				err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		int EnqueueBuffer_mmap(int nIndex) {
			int err;

			switch(1) { case 1:
				qvio_buffer args = pMmapBufs[nIndex];
				err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			return err;
		}

		void OnTest1(int64_t now) {
			int err;

			for(int i = 0;i < pMmapBufs.size();i++) {
				err = EnqueueBuffer_mmap(i);
				if(err) {
					LOGE("%s(%d): EnqueueBuffer_mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}

		void OnTest2(int64_t now) {
#if BUILD_WITH_NVBUF
			int err;

			for(int i = 0;i < pNVBuf_surfaces.size();i++) {
				err = EnqueueBuffer_nvbuf(i);
				if(err) {
					LOGE("%s(%d): EnqueueBuffer_nvbuf() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
#endif // BUILD_WITH_NVBUF
		}

		void OnTest3(int64_t now) {
			int err;

			for(int i = 0;i < pSysBufs.size();i++) {
				err = EnqueueBuffer_sysbuf(i);
				if(err) {
					LOGE("%s(%d): EnqueueBuffer_sysbuf() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}

		void OnTest4(int64_t now) {
			int err;

			switch(1) { case 1:
				LOGD("QVIO_IOC_QBUF...");
				int nQbufs = 0;
				for(int i = 0;i < pMmapBufs.size();i++) {
					err = EnqueueBuffer_mmap(i);
					if(err) {
						LOGE("%s(%d): EnqueueBuffer_mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					nQbufs++;
				}
				if(err)
					break;

				ZzUtils::ZzStatBitRate oStatBitRate;
				oStatBitRate.log_prefix = "mmap";
				oStatBitRate.Reset();

				LOGD("QVIO_IOC_STREAMON...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMON);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

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
						// LOGD("%d: QVIO_IOC_DQBUF", i);
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

						oStatBitRate.Log(nWidth * nHeight * 8, now);

						// LOGD("%d: QVIO_IOC_QBUF, nBufIdx=%d", i, nBufIdx);
						err = EnqueueBuffer_mmap(nBufIdx);
						if(err) {
							LOGE("%s(%d): EnqueueBuffer_mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;
					}
				}

				LOGD("QVIO_IOC_STREAMOFF...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMOFF);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}

		void OnTest5(int64_t now) {
#if BUILD_WITH_NVBUF
			int err;

			switch(1) { case 1:
				LOGD("QVIO_IOC_QBUF...");
				int nQbufs = 0;
				for(int i = 0;i < pNVBuf_surfaces.size();i++) {
					err = EnqueueBuffer_nvbuf(i);
					if(err) {
						LOGE("%s(%d): EnqueueBuffer_nvbuf() failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					nQbufs++;
				}
				if(err)
					break;

				ZzUtils::ZzStatBitRate oStatBitRate;
				oStatBitRate.log_prefix = "dmabuf";
				oStatBitRate.Reset();

				LOGD("QVIO_IOC_STREAMON...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMON);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

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
						// LOGD("%d: QVIO_IOC_DQBUF", i);
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

						oStatBitRate.Log(nWidth * nHeight * 8, now);

						// LOGD("%d: QVIO_IOC_QBUF, nBufIdx=%d", i, nBufIdx);
						err = EnqueueBuffer_nvbuf(nBufIdx);
						if(err) {
							LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;
					}
				}

				LOGD("QVIO_IOC_STREAMOFF...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMOFF);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
#endif // BUILD_WITH_NVBUF
		}

		void OnTest6(int64_t now) {
			int err;

			switch(1) { case 1:
				LOGD("QVIO_IOC_QBUF...");
				int nQbufs = 0;
				for(int i = 0;i < pSysBufs.size();i++) {
					err = EnqueueBuffer_sysbuf(i);
					if(err) {
						LOGE("%s(%d): EnqueueBuffer_sysbuf() failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					nQbufs++;
				}
				if(err)
					break;

				ZzUtils::ZzStatBitRate oStatBitRate;
				oStatBitRate.log_prefix = "userptr";
				oStatBitRate.Reset();

				LOGD("QVIO_IOC_STREAMON...");
				err = ioctl(fd_qvio, QVIO_IOC_STREAMON);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

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
						// LOGD("%d: QVIO_IOC_DQBUF", i);
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

						oStatBitRate.Log(nWidth * nHeight * 8, now);

						// LOGD("%d: QVIO_IOC_QBUF, nBufIdx=%d", i, nBufIdx);
						err = EnqueueBuffer_sysbuf(nBufIdx);
						if(err) {
							LOGE("%s(%d): EnqueueBuffer_sysbuf() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;
					}
				}

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

using namespace __06_aximm_test1__;

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
