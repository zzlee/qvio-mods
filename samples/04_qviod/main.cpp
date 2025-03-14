#include "ZzLog.h"
#include "ZzUtils.h"
#include "ZzDeferredTasks.h"
#include "ZzCUDA.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <linux/version.h>

#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <memory>

#include "qvio.h"

#if BUILD_WITH_NPP
#include <npp.h>
#include <nppi.h>
#include <npps.h>
#endif // BUILD_WITH_NPP

#if BUILD_WITH_NVBUF
#include <nvbufsurface.h>
#endif // BUILD_WITH_NVBUF

ZZ_INIT_LOG("04_qviod")

namespace __04_qviod__ {
	struct App {
		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;
		int nVidFd;
		int nVidUserJobFd;

		App(int argc, char **argv);
		~App();

		int Run();

		void OpenVidRx();

		// user job handling
		v4l2_format oVidDstFormat;
		ZzDeferredTasks oDeferredTasks;

		void VidUserJobHandling();
		void VidUserJob_S_FMT(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_QUEUE_SETUP(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_BUF_INIT(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_BUF_CLEANUP(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_START_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_STOP_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_BUF_DONE(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
		void VidUserJob_ERROR(const qvio_user_job& user_job, qvio_user_job_done& user_job_done);
	};

	App::App(int argc, char **argv) : argc(argc), argv(argv) {
		// LOGD("%s(%d):", __FUNCTION__, __LINE__);
	}

	App::~App() {
		// LOGD("%s(%d):", __FUNCTION__, __LINE__);
	}

	int App::Run() {
		int err;

		srand((unsigned)time(NULL));

		switch(1) { case 1:
			nVidFd = -1;
			nVidUserJobFd = -1;

			OpenVidRx();
			VidUserJobHandling();

			err = 0;
		}

		oFreeStack.Flush();

		return err;
	}

	void App::OpenVidRx() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			if(nVidFd != -1) {
				LOGE("%s(%d): unexpected value, nVidFd=%d", __FUNCTION__, __LINE__, nVidFd);
				break;
			}

			nVidFd = open("/dev/video0", O_RDWR | O_NONBLOCK);
			if(nVidFd == -1) {
				err = errno;
				LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			oFreeStack += [&]() {
				int err;

				err = close(nVidFd);
				if(err) {
					err = errno;
					LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
				nVidFd = -1;
			};

			LOGD("nVidFd=%d", nVidFd);

			err = ioctl(nVidFd, QVID_IOC_USER_JOB_FD, &nVidUserJobFd);
			if(err) {
				err = errno;
				LOGE("%s(%d): ioctl(QVID_IOC_USER_JOB_FD) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			oFreeStack += [&]() {
				int err;

				err = close(nVidUserJobFd);
				if(err) {
					err = errno;
					LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
				nVidUserJobFd = -1;
			};

			LOGD("nVidUserJobFd=%d", nVidUserJobFd);
		}
	}

	void App::VidUserJobHandling() {
		int err;

		LOGD("%s(%d):+++", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			cudaFree(NULL);

			err = oDeferredTasks.Start();
			if(err) {
				LOGE("%s(%d): oDeferredTasks.Start() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			int fd_stdin = 0; // stdin

			while(true) {
				fd_set readfds;
				FD_ZERO(&readfds);

				int fd_max = -1;
				if(fd_stdin > fd_max) fd_max = fd_stdin;
				if(nVidUserJobFd > fd_max) fd_max = nVidUserJobFd;
				FD_SET(fd_stdin, &readfds);
				FD_SET(nVidUserJobFd, &readfds);

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

				if (FD_ISSET(nVidUserJobFd, &readfds)) {
					qvio_user_job user_job;
					qvio_user_job_done user_job_done;

					err = ioctl(nVidUserJobFd, QVID_IOC_USER_JOB_GET, &user_job);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVID_IOC_USER_JOB_GET) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}

					switch(user_job.id) {
					case QVIO_USER_JOB_ID_S_FMT:
						VidUserJob_S_FMT(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_QUEUE_SETUP:
						VidUserJob_QUEUE_SETUP(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_BUF_INIT:
						VidUserJob_BUF_INIT(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_BUF_CLEANUP:
						VidUserJob_BUF_CLEANUP(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_START_STREAMING:
						VidUserJob_START_STREAMING(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_STOP_STREAMING:
						VidUserJob_STOP_STREAMING(user_job, user_job_done);
						break;

					case QVIO_USER_JOB_ID_BUF_DONE:
						VidUserJob_BUF_DONE(user_job, user_job_done);
						break;

					default:
						VidUserJob_ERROR(user_job, user_job_done);
						break;
					}

					user_job_done.id = user_job.id;
					user_job_done.sequence = user_job.sequence;

#if 0 // DEBUG
					LOGD("user_job_done(%d, %d)", (int)user_job_done.id, user_job_done.sequence);
#endif

#if 1
					err = ioctl(nVidUserJobFd, QVID_IOC_USER_JOB_DONE, &user_job_done);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVID_IOC_USER_JOB_DONE) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
#endif
				}
			}

			oDeferredTasks.Stop();
		}

		LOGD("%s(%d):---", __FUNCTION__, __LINE__);
	}

	void App::VidUserJob_S_FMT(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): type=%d %dx%d %d", __FUNCTION__, (int)user_job.sequence,
			(int)user_job.u.s_fmt.format.type,
			(int)user_job.u.s_fmt.format.fmt.pix.width,
			(int)user_job.u.s_fmt.format.fmt.pix.height,
			(int)user_job.u.s_fmt.format.fmt.pix.bytesperline);

		oVidDstFormat = user_job.u.s_fmt.format;

		LOGD("oVidDstFormat: %dx%d 0x%X %d %d",
			(int)oVidDstFormat.fmt.pix.width,
			(int)oVidDstFormat.fmt.pix.height,
			oVidDstFormat.fmt.pix.pixelformat,
			(int)oVidDstFormat.fmt.pix.sizeimage,
			(int)oVidDstFormat.fmt.pix.bytesperline);
	}

	void App::VidUserJob_QUEUE_SETUP(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): num_buffers=%d", __FUNCTION__, (int)user_job.sequence, (int)user_job.u.queue_setup.num_buffers);
	}

	void App::VidUserJob_BUF_INIT(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_init.index);
	}

	void App::VidUserJob_BUF_CLEANUP(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_cleanup.index);
	}

	void App::VidUserJob_START_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): ", __FUNCTION__, (int)user_job.sequence);
	}

	void App::VidUserJob_STOP_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): ", __FUNCTION__, (int)user_job.sequence);
	}

	void App::VidUserJob_BUF_DONE(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_done.index);
	}

	void App::VidUserJob_ERROR(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): id=%d", __FUNCTION__, (int)user_job.sequence, (int)user_job.id);
	}
}

using namespace __04_qviod__;

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
