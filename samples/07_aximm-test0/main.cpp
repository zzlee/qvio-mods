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

#define CONCAT_I(N, S) N ## S
#define CONCAT(N, S) CONCAT_I(N, S)
#define GUARD_NAME CONCAT(__GUARD_, __LINE__)

ZZ_INIT_LOG("07_aximm-test0")

using namespace __zz_clock__;

namespace __07_aximm_test0__ {
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
		int nSize;
		int nTimes;

		int fd_qvio;
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
			nSize = nStride;
			nTimes = nHeight;

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
					int work_mode = QVIO_WORK_MODE_AXIMM_TEST0;
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

						switch(ch) {
						case '1':
							OnTest1(now);
							break;
						}
					}

					return err;
				});
			}

			return err;
		}

		void OnTest1(int64_t now) {
			int err;

			switch(1) { case 1:
				LOGD("QVIO_IOC_QBUF...");
				int nQbufs = 0;
				{
					qvio_buffer args;

					for(int i = 0;i < pSysBufs.size();i++) {
						memset(&args, 0, sizeof(args));
						args.index = i;
						args.buf_type = QVIO_BUF_TYPE_USERPTR;
						args.u.userptr = (unsigned long)pSysBufs[i];
						args.offset[0] = 0;
						args.stride[0] = nStride;
						err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
						if(err) {
							err = errno;
							LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
						nQbufs++;

						LOGD("%d: args.index=%d", i, args.index);
					}
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
						{
							qvio_buffer args;

							memset(&args, 0, sizeof(args));
							args.index = nBufIdx;
							args.buf_type = QVIO_BUF_TYPE_USERPTR;
							args.u.userptr = (uintptr_t)pSysBufs[nBufIdx];
							args.offset[0] = 0;
							args.stride[0] = nStride;
							err = ioctl(fd_qvio, QVIO_IOC_QBUF, &args);
							if(err) {
								err = errno;
								LOGE("%s(%d): ioctl(QVIO_IOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
								break;
							}
							nQbufs++;
						}
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

using namespace __07_aximm_test0__;

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
