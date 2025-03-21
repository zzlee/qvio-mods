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
	struct App {
		typedef App self_t;

		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;

		int fd_qvio;
		bool bMeasureTicks;
		ZzUtils::RateCtrl oRateCtrl;
		int64_t next_time;
		int64_t last_tick_time;
		__u32 last_ticks;
		std::vector<NvBufSurface*> pNVBuf_surfaces;

		App(int argc, char **argv) : argc(argc), argv(argv) {
		}

		~App() {
		}

		int Run() {
			int err;
			int64_t now = _clk();

			LOGD("%s::%s", typeid(self_t).name(), __FUNCTION__);

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

				pNVBuf_surfaces.resize(4);

				NvBufSurfaceCreateParams oNVBufParam;
				memset(&oNVBufParam, 0, sizeof(oNVBufParam));
				oNVBufParam.width = 4096;
				oNVBufParam.height = 2160;
				oNVBufParam.layout = NVBUF_LAYOUT_PITCH;
				oNVBufParam.memType = NVBUF_MEM_DEFAULT;
				oNVBufParam.gpuId = 0;
				oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_NV12;
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

						case '1':
							OnTest1(now);
							break;

						case '2':
							OnTest2(now);
							break;

						case '3':
							OnTest3(now);
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

		void OnTest1(int64_t now) {
			int err;

			switch(1) { case 1:
				qvio_s_fmt args;
				args.width = 4096;
				args.height = 2160;
				args.fmt = 0;
				err = ioctl(fd_qvio, QVIO_IOC_S_FMT, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}

		void OnTest2(int64_t now) {
			int err;

			switch(1) { case 1:
				qvio_g_fmt args;
				err = ioctl(fd_qvio, QVIO_IOC_G_FMT, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QVIO_IOC_G_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("%dx%d 0x%08X", args.width, args.height, args.fmt);
			}
		}

		void OnTest3(int64_t now) {
			int err;

			switch(1) { case 1:
				qvio_qbuf args;

				for(int i = 0;i < pNVBuf_surfaces.size();i++) {
					NvBufSurfaceParams& surfaceParams = pNVBuf_surfaces[i]->surfaceList[0];

					args.cookie = (uintptr_t)i;
					args.u.dmabuf = (int)surfaceParams.bufferDesc;
					args.offset[0] = surfaceParams.planeParams.offset[0];
					args.offset[1] = surfaceParams.planeParams.offset[1];
					args.stride[0] = surfaceParams.planeParams.pitch[0];
					args.stride[1] = surfaceParams.planeParams.pitch[1];
					err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
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
