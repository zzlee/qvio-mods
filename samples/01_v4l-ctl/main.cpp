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
#include <linux/videodev2.h>
#include <linux/media.h>
#include <linux/version.h>
#include <linux/limits.h>
#include <vector>
#include <fstream>

#include "qvio.h"

ZZ_INIT_LOG("01_v4l-ctl")

using namespace __zz_clock__;

namespace __01_v4l_ctl__ {
	struct App {
		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;
		int nVidFd;
		int nMemory;
		int nBufType;
		int nPixelFormat;
		int nWidth;
		int nHeight;
		int nHAlign;
		int nVAlign;
		int nBuffers;

		struct mmap_buffer {
			intptr_t pVirAddr[4];
			int nLength[4];
		};
		std::vector<mmap_buffer> oMbuffers;

		struct exp_buffer {
			int nFd[4];
		};
		std::vector<exp_buffer> oDmaBufs;

		App(int argc, char **argv);
		~App();

		int Run();

		void OpenVidRx();
		void VidSFmt();
		void VidReqBufs();
		void VidQueryBufs();
		void VidMmap();
		void MmapWriteTest();
		void MmapReadTest();
		void VidExpBufs();
		void VidStreamOn();
		void VidStreamOff();
		void VidQbufs();
		void VidWaitForBuffer(int nFrames);
		void VidBufDone();
		void VidMeasureFPS();
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
			nMemory = V4L2_MEMORY_MMAP;
			nBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			nPixelFormat = V4L2_PIX_FMT_M420;
			nWidth = 3840;
			nHeight = 2160;
			nHAlign = ZZ_ALIGN(nWidth, 0x40);
			nVAlign = ZZ_ALIGN(nHeight, 0x1);
			nBuffers = 4;

			LOGD("param: %d %d %d %d %d", nMemory, nBufType, nWidth, nHeight,
				nHAlign, nVAlign, nBuffers);

			ZzUtils::TestLoop([&](int ch) -> int {
				int err = 0;

				// LOGD("ch=%d", ch);

				switch(ch) {
				case 'r':
				case 'R':
					OpenVidRx();
					break;

				case '1':
					VidSFmt();
					break;

				case '2':
					VidReqBufs();
					break;

				case '3':
					VidQueryBufs();
					break;

				case '4':
					VidMmap();
					break;

				case '5':
					VidExpBufs();
					break;

				case '6':
					VidQbufs();
					break;

				case '7':
					VidStreamOn();
					break;

				case '8':
					VidStreamOff();
					break;

				case '9':
					VidBufDone();
					break;

				case '0':
					VidMeasureFPS();
					break;

				case 'a':
				case 'A':
					VidWaitForBuffer(1);
					break;
				}

				return err;
			}, 1000000LL, 1LL);

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
		}
	}

	void App::VidSFmt() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			v4l2_format format;
			memset(&format, 0, sizeof(v4l2_format));
			format.type = nBufType;
			format.fmt.pix.width = nWidth;
			format.fmt.pix.height = nHeight;
			format.fmt.pix.pixelformat = nPixelFormat;
			format.fmt.pix.field = V4L2_FIELD_NONE;

			LOGD("+param: %d %d",
				(int)format.fmt.pix.sizeimage,
				(int)format.fmt.pix.bytesperline);

			err = ioctl(nVidFd, VIDIOC_S_FMT, &format);
			if(err) {
				err = errno;
				LOGE("%s(%d): ioctl(VIDIOC_S_FMT) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			LOGD("-param: %d %d",
				(int)format.fmt.pix.sizeimage,
				(int)format.fmt.pix.bytesperline);
		}
	}

	void App::VidReqBufs() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			struct v4l2_requestbuffers requestBuffers;
			memset(&requestBuffers, 0, sizeof(struct v4l2_requestbuffers));
			requestBuffers.count = nBuffers;
			requestBuffers.type = nBufType;
			requestBuffers.memory = nMemory;
			err = ioctl(nVidFd, VIDIOC_REQBUFS, &requestBuffers);
			if(err) {
				err = errno;
				LOGE("%s(%d): ioctl(VIDIOC_REQBUFS) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			LOGD("param: %d", requestBuffers.count);
		}
	}

	void App::VidQueryBufs() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			for(int i = 0;i < nBuffers;i++) {
				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.index = i;
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("BUFFER[%d]: length=%d offset=%d",
					buffer.index, buffer.length, (int)buffer.m.offset);
			}
		}
	}

	void App::VidMmap() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			if(! oMbuffers.empty()) {
				LOGE("%s(%d): unexpected, oMbuffers.size()=%d", __FUNCTION__, __LINE__, (int)oMbuffers.size());
				break;
			}

			for(int i = 0;i < nBuffers;i++) {
				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.index = i;
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				mmap_buffer mbuffer;
				memset(&mbuffer, 0, sizeof(mmap_buffer));
				{
					size_t nLength = (size_t)buffer.length;
					void* pVirAddr = mmap(NULL, nLength, PROT_READ | PROT_WRITE, MAP_SHARED,
						nVidFd, (off_t)buffer.m.offset);
					if(pVirAddr == MAP_FAILED) {
						LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					oFreeStack += [pVirAddr, nLength]() {
						int err;

						err = munmap(pVirAddr, nLength);
						if(err) {
							err = errno;
							LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
						}
					};

					mbuffer.pVirAddr[0] = (intptr_t)pVirAddr;
					mbuffer.nLength[0] = nLength;
				}

				oMbuffers.push_back(mbuffer);

				LOGD("BUFFER[%d]: length=%d offset=%d [%p]",
					buffer.index, buffer.length, (int)buffer.m.offset,
					mbuffer.pVirAddr[0]);
			}
		}
	}

	void App::MmapWriteTest() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			if(oMbuffers.empty()) {
				LOGE("%s(%d): unexpected, oMbuffers.size()=%d", __FUNCTION__, __LINE__, (int)oMbuffers.size());
				break;
			}

			mmap_buffer mbuffer = oMbuffers[0];
			int nVal0 = rand() % 256;
			int nVal1 = rand() % 256;

			LOGD("param: [%d %d] [%p %p] [%d %d]",
				nVal0, nVal1, mbuffer.pVirAddr[0], mbuffer.pVirAddr[1],
				mbuffer.nLength[0], mbuffer.nLength[1]);

			memset((void*)mbuffer.pVirAddr[0], nVal0, mbuffer.nLength[0]);
			memset((void*)mbuffer.pVirAddr[1], nVal1, mbuffer.nLength[1]);
		}
	}

	void App::MmapReadTest() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			if(oMbuffers.empty()) {
				LOGE("%s(%d): unexpected, oMbuffers.size()=%d", __FUNCTION__, __LINE__, (int)oMbuffers.size());
				break;
			}

			mmap_buffer mbuffer = oMbuffers[0];

			uint8_t* pVal0 = (uint8_t*)mbuffer.pVirAddr[0];
			uint8_t* pVal1 = (uint8_t*)mbuffer.pVirAddr[1];
			int nVal0 = (int)pVal0[mbuffer.nLength[0] / 2];
			int nVal1 = (int)pVal1[mbuffer.nLength[1] / 2];

			LOGD("param: [%d %d] [%p %p] [%d %d]",
				nVal0, nVal1, mbuffer.pVirAddr[0], mbuffer.pVirAddr[1],
				mbuffer.nLength[0], mbuffer.nLength[1]);
		}
	}

	void App::VidExpBufs() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			if(! oDmaBufs.empty()) {
				LOGE("%s(%d): unexpected, oDmaBufs.size()=%d", __FUNCTION__, __LINE__, (int)oDmaBufs.size());
				break;
			}

			for(int i = 0;i < nBuffers;i++) {
				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.index = i;
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				exp_buffer ebuffer;
				memset(&ebuffer, 0, sizeof(exp_buffer));
				{
					struct v4l2_exportbuffer expbuf;

					memset(&expbuf, 0, sizeof(expbuf));
					expbuf.type = nBufType;
					expbuf.index = i;
					expbuf.plane = 0;
					err = ioctl(nVidFd, VIDIOC_EXPBUF, &expbuf);
					if(err) {
						err = errno;
						LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
						break;
					}
					oFreeStack += [expbuf]() {
						int err;

						err = close(expbuf.fd);
						if(err) {
							err = errno;
							LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
						}
					};

					ebuffer.nFd[0] = expbuf.fd;
				}

				oDmaBufs.push_back(ebuffer);

				LOGD("BUFFER[%d]: length=%d offset=%d [%d]",
					buffer.index, buffer.length, (int)buffer.m.offset,
					ebuffer.nFd[0]);
			}
		}
	}

	void App::VidStreamOn() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			enum v4l2_buf_type buf_type = (v4l2_buf_type)nBufType;
			err = ioctl(nVidFd, VIDIOC_STREAMON, &buf_type);
			if(err) {
				LOGE("%s(%d): ioctl(VIDIOC_STREAMON) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
		}
	}

	void App::VidStreamOff() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			enum v4l2_buf_type buf_type = (v4l2_buf_type)nBufType;
			err = ioctl(nVidFd, VIDIOC_STREAMOFF, &buf_type);
			if(err) {
				LOGE("%s(%d): ioctl(VIDIOC_STREAMOFF) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
		}
	}

	void App::VidBufDone() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			err = ioctl(nVidFd, QVID_IOC_BUF_DONE);
			if(err) {
				LOGE("%s(%d): ioctl(QVID_IOC_BUF_DONE) failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
		}
	}

	void App::VidQbufs() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			for(int i = 0;i < nBuffers;i++) {
				v4l2_buffer buffer;
				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.index = i;
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_QUERYBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QUERYBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				err = ioctl(nVidFd, VIDIOC_QBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}
	}

	void App::VidWaitForBuffer(int nFrames) {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		int fd_stdin = 0; // stdin
		int64_t beg = _clk();

		LOGW("Wait for test... %d", nFrames);
		while(true) {
			fd_set readfds;
			FD_ZERO(&readfds);

			int fd_max = -1;
			if(fd_stdin > fd_max) fd_max = fd_stdin;
			FD_SET(fd_stdin, &readfds);
			if(nVidFd > fd_max) fd_max = nVidFd;
			FD_SET(nVidFd, &readfds);

			err = select(fd_max + 1, &readfds, NULL, NULL, NULL);
			if (err < 0) {
				LOGE("%s(%d): select() failed! err = %d", __FUNCTION__, __LINE__, err);
				break;
			}

			if (FD_ISSET(fd_stdin, &readfds)) {
				int ch = getchar();

				if(ch == 'q')
					break;
			}

			if (FD_ISSET(nVidFd, &readfds)) {
				v4l2_buffer buffer;

				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_DQBUF, &buffer);
				if(err) {
					LOGE("%s(%d): ioctl(VIDIOC_DQBUF) failed! err = %d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("VIDIOC_DQBUF, buffer.index=%d", buffer.index);

				{
					mmap_buffer mbuffer = oMbuffers[buffer.index];

					char fn[PATH_MAX];
					sprintf(fn, "buffer-%d.bin", buffer.index);
					std::ofstream ofs(fn, std::ios::binary);
					ofs.write((const char*)mbuffer.pVirAddr[0], mbuffer.nLength[0]);
				}

				buffer.flags = 0;
				err = ioctl(nVidFd, VIDIOC_QBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				if(--nFrames <= 0)
					break;
			}
		}

		int64_t fini = _clk();
		LOGW("Test done. %.4fs", (fini - beg) / 1000000.0);
	}

	void App::VidMeasureFPS() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		int fd_stdin = 0; // stdin
		int64_t beg = _clk();
		int64_t now = beg;

		ZzUtils::ZzStatBitRate bitrate;
		bitrate.log_prefix = "vsrc";
		bitrate.Reset();

		ZzUtils::ZzStatBitRate ticks;
		ticks.log_prefix = "ticks";
		ticks.Reset();

		VidStreamOn();

		LOGW("Wait for test...");
		while(true) {
			fd_set readfds;
			FD_ZERO(&readfds);

			int fd_max = -1;
			if(fd_stdin > fd_max) fd_max = fd_stdin;
			FD_SET(fd_stdin, &readfds);
			if(nVidFd > fd_max) fd_max = nVidFd;
			FD_SET(nVidFd, &readfds);

			err = select(fd_max + 1, &readfds, NULL, NULL, NULL);
			if (err < 0) {
				LOGE("%s(%d): select() failed! err = %d", __FUNCTION__, __LINE__, err);
				break;
			}

			now = _clk();
			ticks.Log(1, now);

			if (FD_ISSET(fd_stdin, &readfds)) {
				int ch = getchar();

				if(ch == 'q')
					break;
			}

			if (FD_ISSET(nVidFd, &readfds)) {
				v4l2_buffer buffer;

				memset(&buffer, 0, sizeof(v4l2_buffer));
				buffer.type = nBufType;
				buffer.memory = nMemory;
				err = ioctl(nVidFd, VIDIOC_DQBUF, &buffer);
				if(err) {
					LOGE("%s(%d): ioctl(VIDIOC_DQBUF) failed! err = %d", __FUNCTION__, __LINE__, err);
					break;
				}

				mmap_buffer mbuffer = oMbuffers[buffer.index];
				bitrate.Log(mbuffer.nLength[0] * 8, now);

				buffer.flags = 0;
				err = ioctl(nVidFd, VIDIOC_QBUF, &buffer);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(VIDIOC_QBUF) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
		}

		VidStreamOff();

		int64_t fini = _clk();
		LOGW("Test done. %.4fs", (fini - beg) / 1000000.0);
	}
}

using namespace __01_v4l_ctl__;

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
