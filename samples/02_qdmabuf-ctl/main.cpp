#include "ZzLog.h"
#include "ZzUtils.h"

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#if BUILD_WITH_NVBUF
#include <nvbufsurface.h>
#endif // BUILD_WITH_NVBUF

#include "qdmabuf.h"

ZZ_INIT_LOG("02_qdmabuf-ctl")

namespace __02_qdmabuf_ctl__ {

	struct App {
		int argc;
		char **argv;

		App(int argc, char **argv);
		~App();

		int Run();
	};

	App::App(int argc, char **argv) : argc(argc), argv(argv) {
		LOGD("%s(%d):", __FUNCTION__, __LINE__);
	}

	App::~App() {
		LOGD("%s(%d):", __FUNCTION__, __LINE__);
	}

	int App::Run() {
		int err = 0;
		ZzUtils::FreeStack oFreeStack;

		const int buf_size = 4096 * 2160 * 2;
		const int buffer_count = 8;

		int fd_qdmabuf;
		int fd_dma_buf_dma_contig[buffer_count];
		int fd_dma_buf_dma_sg[buffer_count];
		int fd_dma_buf_vmalloc[buffer_count];

		void* dma_buf_mmap_addr[buffer_count];

#if BUILD_WITH_NVBUF
		NvBufSurface* pNVBuf_surface[buffer_count];
		int fd_dma_buf_nvbuf[buffer_count];
#endif

		switch(1) { case 1:
			fd_qdmabuf = open("/dev/qdmabuf0", O_RDWR);
			if(fd_qdmabuf == -1) {
				err = errno;
				LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			oFreeStack += [&]() {
				close(fd_qdmabuf);
			};

#if 0
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_alloc_args args;
				args.len = buf_size;
				args.type = QDMABUF_TYPE_DMA_CONTIG;
				args.fd_flags = O_RDWR | O_CLOEXEC;
				args.dma_dir = QDMABUF_DMA_DIR_BIDIRECTIONAL;
				args.fd = 0;
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_ALLOC, &args);
				if(err < 0) {
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_ALLOC) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("args={.len=%d, .fd=%d}", args.len, args.fd);

				fd_dma_buf_dma_contig[i] = args.fd;
				oFreeStack += [&]() {
					close(fd_dma_buf_dma_contig[i]);
				};
			}

			if(err < 0)
				break;

#if 0
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_info_args args;
				args.fd = fd_dma_buf_dma_contig[i];
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_INFO, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_INFO) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}

			if(err < 0)
				break;
#endif

			for(int i = 0;i < buffer_count;i++) {
				int dma_buf_size = buf_size;

				dma_buf_mmap_addr[i] = mmap(NULL, dma_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dma_buf_dma_contig[i], 0);
				if(dma_buf_mmap_addr[i] == MAP_FAILED) {
					err = errno;
					LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("dma_buf_mmap_addr[%d]=%p", i, dma_buf_mmap_addr[i]);
			}

			if(err < 0)
				break;

			for(int i = 0;i < buffer_count;i++) {
				int dma_buf_size = 4096 * 2160 * 2;

				err = munmap(dma_buf_mmap_addr[i], dma_buf_size);
				if(err) {
					err = errno;
					LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			}

			if(err < 0)
				break;
#endif

#if 0
			buf_size = 4 * 1024 * 1024;
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_alloc_args args;
				args.len = buf_size;
				args.type = QDMABUF_TYPE_DMA_SG;
				args.fd_flags = O_RDWR | O_CLOEXEC;
				args.dma_dir = QDMABUF_DMA_DIR_BIDIRECTIONAL;
				args.fd = 0;
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_ALLOC, &args);
				if(err < 0) {
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_ALLOC) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("args={.len=%d, .fd=%d}", args.len, args.fd);

				fd_dma_buf_dma_sg[i] = args.fd;

				oFreeStack += [&, i]() {
					close(fd_dma_buf_dma_sg[i]);
				};
			}

			if(err < 0)
				break;

			LOGD("+QDMABUF_IOCTL_INFO");
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_info_args args;
				args.fd = fd_dma_buf_dma_sg[i];
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_INFO, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_INFO) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
			LOGD("-QDMABUF_IOCTL_INFO");

			if(err < 0)
				break;
#endif

#if 1
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_alloc_args args;
				args.len = buf_size;
				args.type = QDMABUF_TYPE_VMALLOC;
				args.fd_flags = O_RDWR | O_CLOEXEC;
				args.dma_dir = QDMABUF_DMA_DIR_BIDIRECTIONAL;
				args.fd = 0;
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_ALLOC, &args);
				if(err < 0) {
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_ALLOC) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("args={.len=%d, .fd=%d}", args.len, args.fd);

				fd_dma_buf_vmalloc[i] = args.fd;
				oFreeStack += [&, i]() {
					close(fd_dma_buf_vmalloc[i]);
				};
			}

			if(err < 0)
				break;

			LOGD("+QDMABUF_IOCTL_INFO");
			for(int i = 0;i < buffer_count;i++) {
				qdmabuf_info_args args;
				args.fd = fd_dma_buf_vmalloc[i];
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_INFO, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_INFO) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
			LOGD("-QDMABUF_IOCTL_INFO");

			for(int i = 0;i < buffer_count;i++) {
				int dma_buf_size = buf_size;

				dma_buf_mmap_addr[i] = mmap(NULL, dma_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dma_buf_vmalloc[i], 0);
				if(dma_buf_mmap_addr[i] == MAP_FAILED) {
					err = errno;
					LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				LOGD("dma_buf_mmap_addr[%d]=%p", i, dma_buf_mmap_addr[i]);
			}

			if(err < 0)
				break;

			for(int i = 0;i < buffer_count;i++) {
				int dma_buf_size = buf_size;

				err = munmap(dma_buf_mmap_addr[i], dma_buf_size);
				if(err) {
					err = errno;
					LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			}

			if(err < 0)
				break;
#endif

#if 0
#if BUILD_WITH_NVBUF
			NvBufSurfaceCreateParams oNVBufParam;
			memset(&oNVBufParam, 0, sizeof(oNVBufParam));
			oNVBufParam.width = 4096*2;
			oNVBufParam.height = 2160;
			oNVBufParam.layout = NVBUF_LAYOUT_PITCH;
			oNVBufParam.memType = NVBUF_MEM_DEFAULT;
			oNVBufParam.gpuId = 0;
			oNVBufParam.colorFormat = NVBUF_COLOR_FORMAT_NV12;
			for(int i = 0;i < pNVBuf_surface_size;i++) {
				err = NvBufSurfaceCreate(&pNVBuf_surface[i], 1, &oNVBufParam);
				if(err) {
					err = errno;
					LOGE("%s(%d): NvBufSurfaceCreate() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				NvBufSurfaceParams& surfaceParams = pNVBuf_surface[i]->surfaceList[0];

				LOGD("surfaceParams={.width=%d, .height=%d, .bufferDesc=%d, .pitch=%d(%d/%d), offset=%d/%d, psize=%d/%d}",
					surfaceParams.width, surfaceParams.height, (int)surfaceParams.bufferDesc,
					surfaceParams.pitch, surfaceParams.planeParams.pitch[0], surfaceParams.planeParams.pitch[1],
					surfaceParams.planeParams.offset[0], surfaceParams.planeParams.offset[1],
					surfaceParams.planeParams.psize[0], surfaceParams.planeParams.psize[1]);

				fd_dma_buf_nvbuf[i] = (int)surfaceParams.bufferDesc;
				oFreeStack += [&, i]() {
					int err;

					err = NvBufSurfaceDestroy(pNVBuf_surface[i]);
					if(err) {
						err = errno;
						LOGE("%s(%d): NvBufSurfaceDestroy() failed, err=%d", __FUNCTION__, __LINE__, err);
					}
				};
			}

			if(err < 0)
				break;

			LOGD("+QDMABUF_IOCTL_INFO");
			for(int i = 0;i < pNVBuf_surface_size;i++) {
				qdmabuf_info_args args;
				args.fd = fd_dma_buf_nvbuf[i];
				err = ioctl(fd_qdmabuf, QDMABUF_IOCTL_INFO, &args);
				if(err) {
					err = errno;
					LOGE("%s(%d): ioctl(QDMABUF_IOCTL_INFO) failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}
			}
			LOGD("-QDMABUF_IOCTL_INFO");

			if(err < 0)
				break;
#endif // BUILD_WITH_NVBUF
#endif

			ZzUtils::TestLoop([&](int ch) -> int {
				return 0;
			}, 1000000LL, 60LL);
		}

		oFreeStack.Flush();

		return err;
	}
}

using namespace __02_qdmabuf_ctl__;

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
