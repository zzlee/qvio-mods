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

ZZ_INIT_LOG("03_qvio-ctl")

namespace __03_qvio_ctl__ {

	struct App {
		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;
		int nVidFd;
		int nVidUserJobFd;
		int nMemory;
		int nBufType;

		struct mmap_buffer {
			int nIndex;
			intptr_t pVirAddr[4];
			int nLength[4];
		};
		std::vector<mmap_buffer> oMbuffers;

		struct vpss_bufs {
			// src
			NvBufSurface* pSurface;
			CUgraphicsResource cuResource;
			CUeglFrame eglFrame;

			// vpss
			Npp8u* pSrc; // YUV422P
			int nSrcStep;
			Npp8u* pScaled; // YUV422P
			int nScaledStep;
			Npp8u* pScaled1; // NV12 (CbCr plane only)
			int nScaled1Step;
		};
		vpss_bufs oVpssBufs;

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

		// video source
		int nVidSrcFd;
		v4l2_format oVidSrcFormat;
		std::atomic<bool> bVidSrcDone;
		std::thread oVidSrcThread;
		std::vector<mmap_buffer> oSrcMbuffers;
		std::mutex oSrcBufferQMutex;
		std::deque<v4l2_buffer> oSrcBufferQ; // defer tasks for done-ioctl
		void VidSrc_Main();
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
			nVidSrcFd = -1;

			nMemory = V4L2_MEMORY_MMAP;
			nBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			memset(&oVidSrcFormat, 0, sizeof(v4l2_format));
			oVidSrcFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			oVidSrcFormat.fmt.pix.width = 1920;
			oVidSrcFormat.fmt.pix.height = 1080;
			oVidSrcFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
			oVidSrcFormat.fmt.pix.field = V4L2_FIELD_NONE;

#if 0
			ZzUtils::TestLoop([&](int ch) -> int {
				int err = 0;

				// LOGD("ch=%d", ch);

				switch(ch) {
				case 'r':
				case 'R':
					OpenVidRx();
					break;

				case 'd':
				case 'D':
					VidUserJobHandling();
					break;
				}

				return err;
			}, 1000000LL, 1LL);
#else
			OpenVidRx();
			VidUserJobHandling();
#endif

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

			nVidFd = open("/dev/video2", O_RDWR | O_NONBLOCK);
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
			nBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

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
		int err;

		LOGD("%s(%d): num_buffers=%d", __FUNCTION__, (int)user_job.sequence, (int)user_job.u.queue_setup.num_buffers);

		int nNumBuffers = (int)user_job.u.queue_setup.num_buffers;

		mmap_buffer mbuffer;
		memset(&mbuffer, 0, sizeof(mmap_buffer));
		oMbuffers.resize(nNumBuffers, mbuffer);
	}

	void App::VidUserJob_BUF_INIT(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		int err;

		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_init.index);

#if 1
		int nIndex = (int)user_job.u.buf_init.index;
		if(nIndex >= 0 && nIndex < oMbuffers.size()) {
			oDeferredTasks.AddTask([&, nIndex]() {
				LOGD("VIDIOC_QUERYBUF(%d)...", nIndex);

				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.index = nIndex;
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					return;
				}

				LOGD("BUFFER[%d]: length=%d offset=%d",
					buffer.index, buffer.length, buffer.m.offset);

				mmap_buffer& mbuffer = oMbuffers[nIndex];
				memset(&mbuffer, 0, sizeof(mmap_buffer));

				mbuffer.nIndex = buffer.index;

				size_t nLength = (size_t)buffer.length;
				void* pVirAddr = mmap(NULL, nLength, PROT_READ | PROT_WRITE, MAP_SHARED,
					nVidFd, (off_t)buffer.m.offset);
				if(pVirAddr == MAP_FAILED) {
					LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
					return;
				}

				mbuffer.pVirAddr[0] = (intptr_t)pVirAddr;
				mbuffer.nLength[0] = buffer.length;

				LOGD("BUFFER[%d]: mmap [%p %d]",
					buffer.index, mbuffer.pVirAddr[0], mbuffer.nLength[0]);
			});
		}
#endif
	}

	void App::VidUserJob_BUF_CLEANUP(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		int err;

		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_cleanup.index);

#if 1
		int nIndex = (int)user_job.u.buf_init.index;
		if(nIndex >= 0 && nIndex < oMbuffers.size()) {
			oDeferredTasks.AddTask([&, nIndex]() {
				LOGD("munmap BUFFER(%d)...", nIndex);

				mmap_buffer& mbuffer = oMbuffers[nIndex];

				for(int p = 0;p < 4;p++) {
					if(mbuffer.pVirAddr[p] && mbuffer.nLength[p]) {
						LOGD("BUFFER[%d]: munmap p=%d [%p %d]",
							nIndex, p, mbuffer.pVirAddr[p], mbuffer.nLength[p]);

						err = munmap((void*)mbuffer.pVirAddr[p], mbuffer.nLength[p]);
						if(err) {
							err = errno;
							LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
						}
					}
				}

				memset(&mbuffer, 0, sizeof(mmap_buffer));
			});
		}
#endif
	}

	void App::VidUserJob_START_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): ", __FUNCTION__, (int)user_job.sequence);

		oDeferredTasks.AddTask([&]() {
			bVidSrcDone.store(false);
			std::thread t(std::bind(&App::VidSrc_Main, this));
			oVidSrcThread.swap(t);
		});
	}

	void App::VidUserJob_STOP_STREAMING(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): ", __FUNCTION__, (int)user_job.sequence);

		bVidSrcDone.store(true);

		oDeferredTasks.AddTask([&]() {
			oVidSrcThread.join();
		});
	}

	void App::VidUserJob_BUF_DONE(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		int err;
		cudaError_t cuErr;
		NppStatus nppErr;

#if 0
		LOGD("%s(%d): index=%d", __FUNCTION__, (int)user_job.sequence, user_job.u.buf_done.index);
#endif

		v4l2_buffer src_buffer;

		{
			std::lock_guard<std::mutex> _{oSrcBufferQMutex};

			if(oSrcBufferQ.empty()) {
				LOGE("%s(%d): unexpected, oSrcBufferQ.empty()", __FUNCTION__, __LINE__);
				src_buffer.index = -1;
			} else {
				src_buffer = oSrcBufferQ.front();
				oSrcBufferQ.pop_front();
			}
		}

		int nIndex = (int)user_job.u.buf_done.index;
		if(nIndex >= 0 && nIndex < oMbuffers.size() &&
			src_buffer.index >= 0 && src_buffer.index < oSrcMbuffers.size()) switch(1) { case 1:
			mmap_buffer& mbuffer_dst = oMbuffers[nIndex];
			mmap_buffer& mbuffer_src = oSrcMbuffers[src_buffer.index];

#if 0 // DEBUG
			LOGD("SRC-BUFFER[%d] (%p %d) ==> BUFFER[%d] (%p %d)",
				mbuffer_src.nIndex, mbuffer_src.pVirAddr[0], mbuffer_src.nLength[0],
				mbuffer_dst.nIndex, mbuffer_dst.pVirAddr[0], mbuffer_dst.nLength[0]);
#endif

#if 1
			cuErr = cudaMemcpy2D((void*)oVpssBufs.eglFrame.frame.pPitch[0],
				(int)oVpssBufs.pSurface->surfaceList[0].planeParams.pitch[0],
				(const void*)mbuffer_src.pVirAddr[0], oVidSrcFormat.fmt.pix.bytesperline,
				oVidSrcFormat.fmt.pix.width * 2, oVidSrcFormat.fmt.pix.height,
				cudaMemcpyHostToDevice);
			if(cuErr != cudaSuccess) {
				LOGE("%s(%d): cudaMemcpy2D() failed, cuErr=%d", __FUNCTION__, __LINE__, cuErr);
				break;
			}
#endif

			// YUYV(oVpssBufs.eglFrame.frame.pPitch[0]) -> YUV422P(oVpssBufs.pSrc)
#if 1
			{
				uchar1* pDst[3] = {
					(uchar1*)oVpssBufs.pSrc,
					(uchar1*)oVpssBufs.pSrc + oVpssBufs.nSrcStep * oVidSrcFormat.fmt.pix.height,
					(uchar1*)oVpssBufs.pSrc + oVpssBufs.nSrcStep * oVidSrcFormat.fmt.pix.height * 2,
				};
				int nDstStep[3] = {
					oVpssBufs.nSrcStep,
					oVpssBufs.nSrcStep,
					oVpssBufs.nSrcStep,
				};

				cuErr = zppiYCbCr422_8u_C2P3R((uchar1*)oVpssBufs.eglFrame.frame.pPitch[0],
					(int)oVpssBufs.pSurface->surfaceList[0].planeParams.pitch[0], pDst, nDstStep,
					oVidSrcFormat.fmt.pix.width, oVidSrcFormat.fmt.pix.height);
				if(cuErr != cudaSuccess) {
					LOGE("%s(%d): zppiYCbCr422_8u_C2P3R() failed, cuErr=%d", __FUNCTION__, __LINE__, cuErr);
					break;
				}
			}
#endif

			// image resize
			// YUV422P(oVpssBufs.pSrc) --> YUV422P(oVpssBufs.pScaled)
			{
				Npp8u* pSrc = oVpssBufs.pSrc;
				Npp8u* pDst = oVpssBufs.pScaled;

				NppiSize oSrcSize = { (int)oVidSrcFormat.fmt.pix.width, (int)oVidSrcFormat.fmt.pix.height };
				NppiRect oSrcRectROI = { 0, 0, oSrcSize.width, oSrcSize.height };
				NppiSize oDstSize = { (int)oVidDstFormat.fmt.pix.width, (int)oVidDstFormat.fmt.pix.height };
				NppiRect oDstRectROI = { 0, 0, oDstSize.width, oDstSize.height };

				nppErr = nppiResize_8u_C1R(pSrc, oVpssBufs.nSrcStep, oSrcSize, oSrcRectROI,
					pDst, oVpssBufs.nScaledStep, oDstSize, oDstRectROI, NPPI_INTER_NN);
				if(nppErr != NPP_SUCCESS) {
					LOGE("%s(%d): nppiResize_8u_C1R() failed, nppErr=%d", __FUNCTION__, __LINE__, nppErr);
					break;
				}

				// Cb
				pSrc += oSrcSize.height * oVpssBufs.nSrcStep;
				pDst += oDstSize.height * oVpssBufs.nScaledStep;
				oSrcSize.width /= 2, oSrcRectROI.width /= 2;
				oDstSize.width /= 2, oDstRectROI.width /= 2;

				nppErr = nppiResize_8u_C1R(pSrc, oVpssBufs.nSrcStep, oSrcSize, oSrcRectROI,
					pDst, oVpssBufs.nScaledStep, oDstSize, oDstRectROI, NPPI_INTER_NN);
				if(nppErr != NPP_SUCCESS) {
					LOGE("%s(%d): nppiResize_8u_C1R() failed, nppErr=%d", __FUNCTION__, __LINE__, nppErr);
					break;
				}

				// Cr
				pSrc += oSrcSize.height * oVpssBufs.nSrcStep;
				pDst += oDstSize.height * oVpssBufs.nScaledStep;

				nppErr = nppiResize_8u_C1R(pSrc, oVpssBufs.nSrcStep, oSrcSize, oSrcRectROI,
					pDst, oVpssBufs.nScaledStep, oDstSize, oDstRectROI, NPPI_INTER_NN);
				if(nppErr != NPP_SUCCESS) {
					LOGE("%s(%d): nppiResize_8u_C1R() failed, nppErr=%d", __FUNCTION__, __LINE__, nppErr);
					break;
				}
			}

			// YUV422P(oVpssBufs.pScaled Cb+Cr planes) -> NV12(oVpssBufs.pScaled1 CbCr plane)
#if 1
			{
				uchar1* pSrc[2] = {
					(uchar1*)oVpssBufs.pScaled + oVpssBufs.nScaledStep * oVidDstFormat.fmt.pix.height,
					(uchar1*)oVpssBufs.pScaled + oVpssBufs.nScaledStep * oVidDstFormat.fmt.pix.height * 2,
				};
				int nSrcStep[2] = {
					oVpssBufs.nScaledStep * 2,
					oVpssBufs.nScaledStep * 2,
				};

				cuErr = zppiCbCr422_CbCr420_8u_P2C2R(pSrc, nSrcStep,
					(uchar1*)oVpssBufs.pScaled1, (int)oVpssBufs.nScaled1Step,
					oVidDstFormat.fmt.pix.width / 2, oVidDstFormat.fmt.pix.height / 2);
				if(cuErr != cudaSuccess) {
					LOGE("%s(%d): zppiCbCr422_CbCr420_8u_P2C2R() failed, cuErr=%d", __FUNCTION__, __LINE__, cuErr);
					break;
				}
			}
#endif

#if 1
			cuErr = cudaMemcpy2D((void*)mbuffer_dst.pVirAddr[0], oVidDstFormat.fmt.pix.bytesperline,
				(const void*)oVpssBufs.pScaled, (int)oVpssBufs.nScaledStep,
				oVidDstFormat.fmt.pix.width, oVidDstFormat.fmt.pix.height,
				cudaMemcpyDeviceToHost);
			if(cuErr != cudaSuccess) {
				LOGE("%s(%d): cudaMemcpy2D() failed, cuErr=%d", __FUNCTION__, __LINE__, cuErr);
				break;
			}

			cuErr = cudaMemcpy2D(
				(void*)(mbuffer_dst.pVirAddr[0] + oVidDstFormat.fmt.pix.height * oVidDstFormat.fmt.pix.bytesperline),
				oVidDstFormat.fmt.pix.bytesperline,
				(const void*)oVpssBufs.pScaled1, (int)oVpssBufs.nScaled1Step,
				oVidDstFormat.fmt.pix.width, oVidDstFormat.fmt.pix.height / 2,
				cudaMemcpyDeviceToHost);
			if(cuErr != cudaSuccess) {
				LOGE("%s(%d): cudaMemcpy2D() failed, cuErr=%d", __FUNCTION__, __LINE__, cuErr);
				break;
			}
#endif
		}
	}

	void App::VidUserJob_ERROR(const qvio_user_job& user_job, qvio_user_job_done& user_job_done) {
		LOGD("%s(%d): id=%d", __FUNCTION__, (int)user_job.sequence, (int)user_job.id);
	}

	void App::VidSrc_Main() {
		int err;
		CUresult cuRes;
		ZzUtils::FreeStack _FreeStack;

		LOGD("+++%s", __FUNCTION__);

		switch(1) { case 1:
			nVidSrcFd = open("/dev/video0", O_RDWR | O_NONBLOCK);
			if(nVidSrcFd == -1) {
				err = errno;
				LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			_FreeStack += [&]() {
				int err;

				err = close(nVidSrcFd);
				if(err) {
					err = errno;
					LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			};

			LOGD("nVidSrcFd=%d", nVidSrcFd);

			err = ioctl(nVidSrcFd, VIDIOC_S_FMT, &oVidSrcFormat);
			if(err) {
				err = errno;
				LOGE("%s(%d): ioctl(VIDIOC_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			LOGD("oVidSrcFormat: %dx%d 0x%X %d %d",
				(int)oVidSrcFormat.fmt.pix.width,
				(int)oVidSrcFormat.fmt.pix.height,
				oVidSrcFormat.fmt.pix.pixelformat,
				(int)oVidSrcFormat.fmt.pix.sizeimage,
				(int)oVidSrcFormat.fmt.pix.bytesperline);

			struct v4l2_requestbuffers requestBuffers;
			memset(&requestBuffers, 0, sizeof(struct v4l2_requestbuffers));
			requestBuffers.count = 4;
			requestBuffers.type = oVidSrcFormat.type;
			requestBuffers.memory = V4L2_MEMORY_MMAP;
			err = ioctl(nVidSrcFd, VIDIOC_REQBUFS, &requestBuffers);
			if(err) {
				err = errno;
				LOGE("%s(%d): ioctl(VIDIOC_REQBUFS) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			oSrcMbuffers.resize(requestBuffers.count);
			_FreeStack += [&]() {
				oSrcMbuffers.clear();
			};

			LOGD("SRC-BUFFER: requestBuffers.count=%d", (int)requestBuffers.count);

			for(int i = 0;i < requestBuffers.count;i++) {
				int nIndex = i;

				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));

				buffer.index = nIndex;
				buffer.type = oVidSrcFormat.type;
				buffer.memory = V4L2_MEMORY_MMAP;
				err = ioctl(nVidSrcFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("SRC-BUFFER[%d]: length=%d offset=%d",
					buffer.index, buffer.length, buffer.m.offset);

				mmap_buffer& mbuffer = oSrcMbuffers[nIndex];
				memset(&mbuffer, 0, sizeof(mmap_buffer));

				size_t nLength = (size_t)buffer.length;
				void* pVirAddr = mmap(NULL, nLength, PROT_READ | PROT_WRITE, MAP_SHARED,
					nVidSrcFd, (off_t)buffer.m.offset);
				if(pVirAddr == MAP_FAILED) {
					LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
					return;
				}
				_FreeStack += [&, nIndex]() {
					mmap_buffer& mbuffer = oSrcMbuffers[nIndex];

					for(int p = 0;p < 4;p++) {
						if(mbuffer.pVirAddr[p] && mbuffer.nLength[p]) {
							LOGD("BUFFER[%d]: munmap p=%d [%p %d]",
								nIndex, p, mbuffer.pVirAddr[p], mbuffer.nLength[p]);

							err = munmap((void*)mbuffer.pVirAddr[p], mbuffer.nLength[p]);
							if(err) {
								err = errno;
								LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
							}
						}
					}

					memset(&mbuffer, 0, sizeof(mmap_buffer));
				};

				mbuffer.nIndex = buffer.index;
				mbuffer.pVirAddr[0] = (intptr_t)pVirAddr;
				mbuffer.nLength[0] = buffer.length;

				LOGD("SRC-BUFFER[%d]: mmap [%p %d]",
					buffer.index, mbuffer.pVirAddr[0], mbuffer.nLength[0]);

				buffer.flags = 0;
				buffer.index = nIndex;
				buffer.type = oVidSrcFormat.type;
				buffer.memory = V4L2_MEMORY_MMAP;
				err = ioctl(nVidSrcFd, VIDIOC_QBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			cudaFree(NULL);

			NvBufSurfaceCreateParams params;
			memset(&params, 0, sizeof(params));
			params.width = oVidSrcFormat.fmt.pix.width;
			params.height = oVidSrcFormat.fmt.pix.height;
			params.layout = NVBUF_LAYOUT_PITCH;
			params.memType = NVBUF_MEM_DEFAULT;
			params.gpuId = 0;
			params.colorFormat = NVBUF_COLOR_FORMAT_YUYV;
			err = NvBufSurfaceCreate(&oVpssBufs.pSurface, 1, &params);
			if(err) {
				err = errno;
				LOGE("%s(%d): NvBufSurfaceCreate() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			_FreeStack += [&]() {
				int err;

				err = NvBufSurfaceDestroy(oVpssBufs.pSurface);
				if(err) {
					err = errno;
					LOGE("%s(%d): NvBufSurfaceDestroy() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			};

			oVpssBufs.pSurface->numFilled = 1;

			err = NvBufSurfaceMapEglImage(oVpssBufs.pSurface, -1);
			if(err) {
				err = errno;
				LOGE("%s(%d): NvBufSurfaceMapEglImage() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			_FreeStack += [&]() {
				int err;

				err = NvBufSurfaceUnMapEglImage(oVpssBufs.pSurface, -1);
				if(err) {
					err = errno;
					LOGE("%s(%d): NvBufSurfaceUnMapEglImage() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			};

			cuRes = cuGraphicsEGLRegisterImage(&oVpssBufs.cuResource,
				(EGLImageKHR)oVpssBufs.pSurface->surfaceList[0].mappedAddr.eglImage,
				CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
			if(cuRes != CUDA_SUCCESS) {
				LOGE("%s(%d): cuGraphicsEGLRegisterImage() failed, cuRes=%d", __FUNCTION__, __LINE__, cuRes);
				break;
			}
			_FreeStack += [&]() {
				cuRes = cuGraphicsUnregisterResource(oVpssBufs.cuResource);
				if(cuRes != CUDA_SUCCESS) {
					LOGE("%s(%d): cuGraphicsUnregisterResource() failed, cuRes=%d", __FUNCTION__, __LINE__, cuRes);
				}
			};

			cuRes = cuGraphicsResourceGetMappedEglFrame(&oVpssBufs.eglFrame, oVpssBufs.cuResource, 0, 0);
			if(cuRes != CUDA_SUCCESS) {
				LOGE("%s(%d): cuGraphicsResourceGetMappedEglFrame() failed, cuRes=%d", __FUNCTION__, __LINE__, cuRes);
				break;
			}

#if 0 // DEBUG
			LOGD("eglFrame: (%p/%p %d/%d)",
				(uintptr_t)oVpssBufs.eglFrame.frame.pPitch[0],
				(uintptr_t)oVpssBufs.eglFrame.frame.pPitch[1],
				(int)oVpssBufs.pSurface->surfaceList[0].planeParams.pitch[0],
				(int)oVpssBufs.pSurface->surfaceList[0].planeParams.pitch[1]);
#endif

			oVpssBufs.pSrc = nppiMalloc_8u_C1(oVidSrcFormat.fmt.pix.width, oVidSrcFormat.fmt.pix.height * 3, &oVpssBufs.nSrcStep);
			if(! oVpssBufs.pSrc) {
				LOGE("%s(%d): nppiMalloc_8u_C1() failed", __FUNCTION__, __LINE__);
				break;
			}
			_FreeStack += [&]() {
				nppiFree(oVpssBufs.pSrc);
			};

			oVpssBufs.pScaled = nppiMalloc_8u_C1(oVidDstFormat.fmt.pix.width, oVidDstFormat.fmt.pix.height * 3, &oVpssBufs.nScaledStep);
			if(! oVpssBufs.pScaled) {
				LOGE("%s(%d): nppiMalloc_8u_C1() failed", __FUNCTION__, __LINE__);
				break;
			}
			_FreeStack += [&]() {
				nppiFree(oVpssBufs.pScaled);
			};

			oVpssBufs.pScaled1 = nppiMalloc_8u_C1(oVidDstFormat.fmt.pix.width, oVidDstFormat.fmt.pix.height / 2, &oVpssBufs.nScaled1Step);
			if(! oVpssBufs.pScaled1) {
				LOGE("%s(%d): nppiMalloc_8u_C1() failed", __FUNCTION__, __LINE__);
				break;
			}
			_FreeStack += [&]() {
				nppiFree(oVpssBufs.pScaled1);
			};

			__u32 buf_type = oVidSrcFormat.type;
			err = ioctl(nVidSrcFd, VIDIOC_STREAMON, &buf_type);
			if(err) {
				LOGE("%s(%d): ioctl(VIDIOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			_FreeStack += [&]() {
				int err;

				__u32 buf_type = oVidSrcFormat.type;
				err = ioctl(nVidSrcFd, VIDIOC_STREAMOFF, &buf_type);
				if(err) {
					LOGE("%s(%d): ioctl(VIDIOC_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			};

			while(! bVidSrcDone.load()) {
				fd_set readfds;
				FD_ZERO(&readfds);

				int fd_max = -1;
				if(nVidSrcFd > fd_max) fd_max = nVidSrcFd;
				FD_SET(nVidSrcFd, &readfds);

				struct timeval tval;
				tval.tv_sec  = 0;
				tval.tv_usec = 500 * 1000LL;

				err = select(fd_max + 1, &readfds, NULL, NULL, &tval);
				if (err < 0) {
					LOGE("%s(%d): select() failed! err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				if(err == 0) {
					LOGE("%s(%d): timed out", __FUNCTION__, __LINE__);
					break;
				}

				if (FD_ISSET(nVidSrcFd, &readfds)) {
					v4l2_buffer buffer;
					memset(&buffer, 0, sizeof(v4l2_buffer));
					buffer.type = oVidSrcFormat.type;
					buffer.memory = V4L2_MEMORY_MMAP;
					err = ioctl(nVidSrcFd, VIDIOC_DQBUF, &buffer);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(VIDIOC_DQBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}

					{
						std::lock_guard<std::mutex> _{oSrcBufferQMutex};

						oSrcBufferQ.push_back(buffer);
					}

					err = ioctl(nVidFd, QVID_IOC_BUF_DONE);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(QVID_IOC_BUF_DONE) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}

					buffer.flags = 0;
					err = ioctl(nVidSrcFd, VIDIOC_QBUF, &buffer);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(VIDIOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
				}
			}
		}

		_FreeStack.Flush();

		LOGD("---%s", __FUNCTION__);
	}
}

using namespace __03_qvio_ctl__;

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
