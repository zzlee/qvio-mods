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
#include <sys/timerfd.h>

#include <vector>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "qvio-l4t.h"

ZZ_INIT_LOG("11_tpg")

using namespace __zz_clock__;

namespace __11_tpg__ {
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
		ZzUtils::RateCtrl oRateCtrl;
		ZzUtils::ZzStatBitRate oBitrate;

		App(int argc, char **argv) : argc(argc), argv(argv) {
		}

		~App() {
		}

		int Run() {
			int err;
			int64_t now = _clk();

			LOGD("%s::%s", typeid(self_t).name(), __FUNCTION__);

			nFmt = fourcc(0, 0, 0, 0); // RGB
			nWidth = 4096;
			nHeight = 2160;
			oRateCtrl.num = 60;
			oRateCtrl.den = 1;

			switch(1) { case 1:
				ZzUtils::Scoped ZZ_GUARD_NAME([&]() {
					oFreeStack.Flush();
				});

				ZzUtils::TestLoop([&](int ch) -> int {
					int err = 0;

					switch(1) { case 1:
						int64_t now = _clk();

						switch(ch) {
						case '1':
							OnTest1(now, "/dev/qtpg0");
							break;
						}
					}

					return err;
				});
			}

			return err;
		}

		void OnTest1(int64_t now, const char* dev_name) {
			int err;

			switch(1) { case 1:
				int fd_tpg = open(dev_name, O_RDWR);
				if(fd_tpg == -1) {
					err = errno;
					LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
				LOGD("open(\"%s\")=%d...\n", dev_name, fd_tpg);
				ZzUtils::Scoped ZZ_GUARD_NAME([fd_tpg, dev_name]() {
					LOGD("close(\"%s\")=%d...\n", dev_name, fd_tpg);
					close(fd_tpg);
				});

				{
					qvio_format args;
					memset(&args, 0, sizeof(args));
					args.fmt = nFmt;
					args.width = nWidth;
					args.height = nHeight;
					err = ioctl(fd_tpg, QVIO_IOC_TPG_S_FMT, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_TPG_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}

				int fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
				if(fd_timer == -1) {
					err = errno;
					LOGE("%s(%d): timerfd_create failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
				ZzUtils::Scoped ZZ_GUARD_NAME([fd_timer]() {
					close(fd_timer);
				});

				// start duration
				int64_t nDuration = 4000; // 4ms
				itimerspec timer_spec {
					.it_interval = { 0, 0 },
					.it_value = {
						.tv_sec = nDuration / 1000000LL,
						.tv_nsec = (nDuration % 1000000LL) * 1000LL
					},
				};
				err = timerfd_settime(fd_timer, 0, &timer_spec, NULL);
				if(err == -1) {
					err = errno;
					LOGE("%s(%d): timerfd_settime() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				{
					qvio_tpg_config args;
					memset(&args, 0, sizeof(args));
					args.bypass = 0;
					err = ioctl(fd_tpg, QVIO_IOC_TPG_STREAMON, &args);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_TPG_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}
				ZzUtils::Scoped ZZ_GUARD_NAME([fd_tpg]() {
					int err;

					err = ioctl(fd_tpg, QVIO_IOC_TPG_STREAMOFF);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVIO_IOC_TPG_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
					}
				});

				int64_t now = _clk();
				oRateCtrl.Start(now);
				oBitrate.log_prefix = "TPG";
				oBitrate.Reset();

				int fd_stdin = 0; // stdin
				bool bErrorPending = false;
				while(true) {
					fd_set readfds;
					FD_ZERO(&readfds);

					int fd_max = -1;
					if(fd_stdin > fd_max) fd_max = fd_stdin;
					FD_SET(fd_stdin, &readfds);
					if(fd_timer > fd_max) fd_max = fd_timer;
					FD_SET(fd_timer, &readfds);

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

					if (FD_ISSET(fd_timer, &readfds)) switch(1) { case 1:
						uint64_t nExpirations;
						err = read(fd_timer, &nExpirations, sizeof(uint64_t));
						if(err == -1) {
							err = errno;
							if(err == EAGAIN) {
								break;
							}

							LOGE("%s(%d): read() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}

						if(err != sizeof(uint64_t)) {
							LOGE("%s(%d): read() failed! err=%d, errno=%d", __FUNCTION__, __LINE__, err, errno);
							break;
						}

						now = _clk();
						switch(1) { case 1:
							if(oRateCtrl.Next(now)) {
								err = ioctl(fd_tpg, QVIO_IOC_TPG_TRIGGER);
								if(err < 0) {
									err = errno;
									if(! bErrorPending) {
										LOGE("%s(%d): ioctl(QVIO_IOC_TPG_TRIGGER) failed, err=%d", __FUNCTION__, __LINE__, err);
										bErrorPending = true;
									}
									break;
								}
								if(bErrorPending) {
									LOGW("%s(%d): recovered from last error", __FUNCTION__, __LINE__);
									bErrorPending = false;
								}

								oBitrate.Log(1, now);
							}
						}

						int64_t nDuration = oRateCtrl.Advance();
						itimerspec timer_spec {
							.it_interval = { 0, 0 },
							.it_value = {
								.tv_sec = nDuration / 1000000LL,
								.tv_nsec = (nDuration % 1000000LL) * 1000LL
							},
						};
						err = timerfd_settime(fd_timer, 0, &timer_spec, NULL);
						if(err == -1) {
							err = errno;
							LOGE("%s(%d): timerfd_settime() failed, err=%d", __FUNCTION__, __LINE__, err);
							break;
						}
					}
				}
			}
		}
	};
}

using namespace __11_tpg__;

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
