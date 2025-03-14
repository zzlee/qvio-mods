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

ZZ_INIT_LOG("05_user_ctrl")

namespace __05_user_ctrl__ {
	inline void RtlFillMemory(uint8_t* pe , int size, uint8_t set_value)
	{
		memset(pe, set_value, size);
	}

	inline void RtlZeroMemory(uint8_t* pe , int size)
	{
		memset(pe, 0x00, size);
	}

	inline void RtlCopyMemory( uint8_t* pe, uint8_t* po, int nFileSize )
	{
		memcpy(pe, po, nFileSize);
	}

	inline void DELAY_100NS(int64_t v) {
		usleep(v / 10);
	}

	static const int SC0710_I2C_MCU_ADR7_0X31 = 0x00000031;

	static const int SC0710_I2C_MCU_ADR7_0X32 = 0x00000032;
	static const int SC0710_I2C_MCU_ADR7_0X42 = 0x00000042;
	static const int SC0710_I2C_MCU_ADR7_0X52 = 0x00000052;
	static const int SC0710_I2C_MCU_ADR7_0X62 = 0x00000062;

	static const int SC0710_I2C_MCU_ADR7_0X33 = 0x00000033;
	static const int SC0710_I2C_MCU_ADR7_0X43 = 0x00000043;
	static const int SC0710_I2C_MCU_ADR7_0X53 = 0x00000053;
	static const int SC0710_I2C_MCU_ADR7_0X63 = 0x00000063;

#pragma pack(push)
#pragma pack(1)

	typedef struct _VIDEOSOURCE
	{
			unsigned char scale; // 0: even, 1: down scale, 2: up scale

			unsigned char bright;

			unsigned char contrast;

			unsigned char sat;

			unsigned char hue;

			unsigned char r_adjust;

			unsigned char g_adjust;

			unsigned char b_adjust;

	} VIDEOSOURCE;

	typedef struct
	{
		unsigned char AudioLineInSel; // REG 57 0x39 default =  3; 0 : PCM only, 1 : line in only, 2 : mic in only, 3 : PCM + line in, 4 : PCM + mic in

		unsigned char pcmvolume;      // REG 58 0x3A HDMI PCM volume, range 0 - 255, default at 0x80, +12 attenuate -

		unsigned char lineinvolume;   // REG 59 0x3B Mic in or Line in volume, range 0 - 255, default at 0x80, +12 db - 96db, 0.5db, 128+24

		unsigned char noisegate;      // REG 60 0x3C

	} AudioCtl;

	typedef union _HDRPACKET
	{
		struct _TVI_CTRL
		{
			AudioCtl	  audioctl_ex;
			unsigned char tvi_mode;  // HDELAY[ 10 - 8 ] Bit 4-6, TVI_MODE[ Bit 0-3 ], TVI_MODE 0: TVI/NTSC/PAL, 1: AHD, 2: CVI
			unsigned char startX;    // HDELAY[  7 - 0 ]
			unsigned char startY;    // VDELAY[  7 - 0 ]
			unsigned char padding[ 23 ];

		} TVI_CTRL;

		unsigned char     HPB0[30]; // HB0_2[ 3 ] + PB0_27[ 27 ]

	} HDRPACKET;

	typedef struct
	{
		uint8_t OnOff; // Set HIGH for active the PTZ function and it will automatically back to Low after execution.

		uint8_t Header;

		uint8_t Address;

		uint8_t Command;

		uint8_t Data1;

		uint8_t Data2;

		uint8_t Data3;

		uint8_t Data4;

	} PTZ;

	typedef union _PTZ_OTHER
	{
		struct _OTHER
		{
			unsigned char	colordepth;   // REG 56 0x38, 0 : 8bit, 5 : 10bit, 6 : 12bit

			AudioCtl		audioctl;     // REG 57~60, ONLY FOR TB710

		//	unsigned char	padding[ 3 ]; // REG 61 0x3D

			unsigned char   EDIDMode;		 		// 0: Default, 1: Copy, 2: Merge

			unsigned char   InFrlRate_KM807DSN;	 	// [7-4]input FRL Rate; [bit 0], 0:DownScale OFF,  1:DownScale ON ;  [bit 1], 1: DownScale From 4096

			unsigned char   Cap_LupThr_FrlRate;		// [7-4]Capture FRL Rate, [3-0]Loop-Through FRL Rate,  0:TMDS mode, 1:3Gbps(3LANE), 2:6Gbps(3LANE), 3:6Gbps(4LANE), 4:8Gbps(4LANE), 5:10Gbps(4LANE)

		} OTHER;

		struct _OTHER_Lite
		{
			//M2 SC400 N1 HDMI
			//M2 SC400 N2 HDMI
			//M2 SC400 N4 HDMI
			//SC410 N2-L HDMI
			//SC410 N4 HDMI Lite

			unsigned char	colordepth;   // REG 56 0x38, 0 : 8bit, 5 : 10bit, 6 : 12bit

			AudioCtl		audioctl;     // REG 57~60, ONLY FOR TB710

			unsigned char	CableLength; // TuneEQ[D3-D0] 0:auto, others:manual level //REG 0x3D

			unsigned char	padding[ 2 ]; // REG 61 0x3E

		} OTHER_Lite;

		PTZ            ptz;

	} PTZ_OTHER;

	typedef struct _MCU_RESOLUCTION
	{
			unsigned short vtotal;

			unsigned short htotal;

			unsigned short vactive;

			unsigned short hactive;

			unsigned char  fps;

			unsigned char  modeflag; //D0 = 1, interlace  D0 = 0, progressive, D1 = 1 (1000/1001), D2 = 1 full range, D3 = 1 (HDCP status),  (D5,D4)== (1,1): BT2020;  (1,0) :BT601; (0,1) : BT709, (0,0) : unknown;

			unsigned char  audiosamplerate;

			unsigned char  colorformat; // for sc700 series pcie bridge.        0 : YUV422, 1 : YUV444, 2 : RGB, 3: YUV420

			unsigned char  hdcponoff; //  // bit0 : 1: Rx without HDCP key,  0: Rx with HDCP key. ,  Bit1 : led on/off,   Bit2, hot plug : 1 connected , 0 unconnected

			unsigned char  hdr2sdr; // 1 :ENABLE CONVERT HDR TO SDR,  0: DISABLE CONVERT HDR TO SDR

			VIDEOSOURCE    vid; //8   //reg 0x12

			HDRPACKET      hdrpacket; //30

			//PTZ            ptz;
			PTZ_OTHER		ptz_other;

	} MCU_RESOLUCTION;

	typedef union _HDMI20_INTERFACE
	{
		struct {

			unsigned char   selport;

		 // unsigned char   padding;

			unsigned char   data_port_num; // original padding , for m.2 710 video data port number

			unsigned short  bitrate;

			MCU_RESOLUCTION resolution[ 16 ];

		} function_name;

	} HDMI20_INTERFACE;

#pragma pack(pop)

	struct UserCtrl {
		int nFd;
		int nMmapSize;
		void* pMmap;
		ZzUtils::FreeStack oFreeStack;

		explicit UserCtrl();
		~UserCtrl();

		int Open(const char* fn);
		void Close();

		uint32_t ReadRegister(int nOffset);
		void WriteRegister(int nOffset, uint32_t nValue);
		bool AccessI2cRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen, uint32_t DELAY_100US);
		bool AccessSlaveDeviceRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen);
		bool AccessMcuRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen);
	};

	struct App {
		int argc;
		char **argv;

		ZzUtils::FreeStack oFreeStack;
		UserCtrl oUserCtrl;

		App(int argc, char **argv);
		~App();

		int Run();

		void OpenUserCtrl();
		void UserCtrl1();
		void UserCtrl2();
		void UserCtrl3();
		void UserCtrl4();
		void UserCtrl5();
		void UserCtrl6();
		void UserCtrl7();
	};

	UserCtrl::UserCtrl() {
		nFd = -1;
		nMmapSize = 0xFFFF;
		pMmap = MAP_FAILED;
	}

	UserCtrl::~UserCtrl() {
	}

	int UserCtrl::Open(const char* fn) {
		int err = 0;

		switch(1) { case 1:
			nFd = open(fn, O_RDWR | O_NONBLOCK);
			if(nFd == -1) {
				err = errno;
				LOGE("%s(%d): open() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			oFreeStack += [&]() {
				int err;

				err = close(nFd);
				if(err) {
					err = errno;
					LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
				nFd = -1;
			};

			LOGD("nFd=%d", nFd);

			pMmap = mmap(NULL, nMmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, nFd, 0);
			if(pMmap == MAP_FAILED) {
				err = errno;
				LOGE("%s(%d): mmap() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			LOGD("pMmap=%p", pMmap);
			oFreeStack += [&]() {
				int err;

				err = munmap(pMmap, nMmapSize);
				if(err) {
					err = errno;
					LOGE("%s(%d): munmap() failed, err=%d", __FUNCTION__, __LINE__, err);
				}
			};
		}

		return err;
	}

	void UserCtrl::Close() {
		oFreeStack.Flush();
	}

	uint32_t UserCtrl::ReadRegister(int nOffset) {
		return *(uint32_t*)((uint8_t*)pMmap + nOffset);
	}

	void UserCtrl::WriteRegister(int nOffset, uint32_t nValue) {
		*(uint32_t*)((uint8_t*)pMmap + nOffset) = nValue;
	}

	bool UserCtrl::AccessI2cRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen, uint32_t DELAY_100US)
	{
		bool is_success = false;

		uint32_t i = 0;

		uint32_t R00000104 = 0x00000000;

		if(nTxLen > 0) {

			WriteRegister(0x00003000 + 0x00000100, 0x00000002); // RESET TX FIFO

			WriteRegister(0x00003000 + 0x00000100, 0x00000001); // ENABLE IIC

			WriteRegister(0x00003000 + 0x00000108, 0x00000100 | bDevAddr);

			for (i = 0; i < nTxLen; i++) {
				int count = 0;
				do {

					R00000104 = ReadRegister(0x00003000 + 0x00000104);

					if((R00000104 & 0x00000010) == 0x00000000) { // FIFO IS NOT FULL

						break;
					}
					DELAY_100NS(10000);

					count++;

				} while (count < 100);

				if(i == nTxLen - 1) {

					if((nTxLen > 0) && (nRxLen > 0)) { // WITHOUT STOP

						WriteRegister(0x00003000 + 0x00000108, 0x00000000 | pTxBuf[i]);
					}
					else {

						WriteRegister(0x00003000 + 0x00000108, 0x00000200 | pTxBuf[i]);
					}
				}
				else {

					WriteRegister(0x00003000 + 0x00000108, pTxBuf[i]);
				}
			}
		}
		if((nTxLen > 0) && (nRxLen > 0) && (DELAY_100US > 0)) { // WRITE THEN READ

			DELAY_100NS(DELAY_100US * 1000);
		}
		else {

			//	DELAY_100NS( 10000 );

			if(nRxLen) {

				uint32_t _delay = 1;

				_delay += nTxLen;

				if(_delay > 10) {

					_delay = 10;
				}
				//according to windows 196.7
				DELAY_100NS((_delay * 100000));
			}
			else {

				DELAY_100NS(10000);
			}
		}

		// CHECK IF TRANSMIT FIFO EMPTY
		//
		if(nTxLen > 0) {

			for (i = 0; i < 10; i++)
			{
				R00000104 = ReadRegister(0x00003000 + 0x00000104);

				if((R00000104 & 0x00000080) == 0x00000080) { // TX FIFO EMPTY

					is_success = true;

					break;
				}
				DELAY_100NS(10000);
			}
			if((R00000104 & 0x00000080) == 0x00000000) { // TX NOT FIFO EMPTY

				LOGE("%s(%d): I2C WRITE ERROR %d.%d!!", __FUNCTION__, __LINE__, nTxLen, nRxLen);

				is_success = false;
			}
		}

		if(nRxLen > 0) {

			WriteRegister(0x00003000 + 0x00000120, 0x0000000F);

			WriteRegister(0x00003000 + 0x00000100, 0x00000002); // RESET TX FIFO

			WriteRegister(0x00003000 + 0x00000100, 0x00000000); // RESET TX FIFO TO NORMAL

			WriteRegister(0x00003000 + 0x00000108, 0x00000101 | bDevAddr); // START + READ ADDR

			WriteRegister(0x00003000 + 0x00000108, 0x00000200 | nRxLen); // DATA

			WriteRegister(0x00003000 + 0x00000100, 0x00000001); // ENABLE IIC

			for (i = 0; i < nRxLen; i++) {

				const int n_max_wait_count = 100;

				int n_wait_count = 0;

				for (n_wait_count = 0; n_wait_count < n_max_wait_count; n_wait_count++) {

					R00000104 = ReadRegister(0x00003000 + 0x00000104);

					if((R00000104 & 0x00000040) == 0x00000000) { // FIFO NOT EMPTY

						break;
					}
					else {

						DELAY_100NS(10000);
					}
				}
				if(n_wait_count >= n_max_wait_count) { // ERROR

					uint32_t j = 0;

					for (j = 0; j < nRxLen; j++) {

						pRxBuf[j] = (uint8_t)(j);
					}
					break;
				}
				else {

					uint32_t R0000010C = ReadRegister(0x00003000 + 0x0000010C);

					pRxBuf[i] = (uint8_t)(R0000010C & 0x000000FF);
				}
			}
		}

		// CHECK IF TRANSMIT FIFO EMPTY
		//
		if(nRxLen > 0) {

			for (i = 0; i < 2; i++) {

				R00000104 = ReadRegister(0x00003000 + 0x00000104);

				if((R00000104 & 0x00000040) == 0x00000040) { // RX FIFO EMPTY

					is_success = true;

					break;
				}
				DELAY_100NS(10000);
			}
			if((R00000104 & 0x00000040) == 0x00) { // FIFO NOT EMPTY

				LOGE("%s(%d): I2C READ ERROR %d.%d!!", __FUNCTION__, __LINE__, nTxLen, nRxLen);

				is_success = false;
			}
		}
		return is_success;
	}

	bool UserCtrl::AccessSlaveDeviceRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen) {
		return AccessI2cRegisterS(bDevAddr << 1, pTxBuf, nTxLen, pRxBuf, nRxLen, 0);
	}

	bool UserCtrl::AccessMcuRegisterS(uint8_t bDevAddr, uint8_t* pTxBuf, uint32_t nTxLen, uint8_t* pRxBuf, uint32_t nRxLen) {
		unsigned int  i2c_proxy_txlen = 0;

		unsigned int  i2c_proxy_rxlen = 0;

		unsigned char i2c_proxy_txbuf[32] = { 0 };

		unsigned char i2c_proxy_rxbuf[32] = { 0 };

		bool       is_success = false;

		if(nTxLen > 0) {

			if(nRxLen > 0) {

				i2c_proxy_txlen = nTxLen + 2;

				i2c_proxy_txbuf[0] = (bDevAddr << 1) | 1;

				i2c_proxy_txbuf[1] = (unsigned char)(nRxLen);

				RtlCopyMemory(i2c_proxy_txbuf + 2, pTxBuf, nTxLen);
			}
			else {

				i2c_proxy_txlen = nTxLen + 1;

				i2c_proxy_txbuf[0] = (bDevAddr << 1) | 0;

				RtlCopyMemory(i2c_proxy_txbuf + 1, pTxBuf, nTxLen);
			}
			is_success = AccessI2cRegisterS(SC0710_I2C_MCU_ADR7_0X33 << 1, i2c_proxy_txbuf, i2c_proxy_txlen, NULL, 0, 0);
			DELAY_100NS(2500000);
		}
		if(nRxLen > 0) {

			i2c_proxy_rxlen = nRxLen;

			RtlFillMemory(i2c_proxy_rxbuf, nRxLen, 0x00);

			is_success = AccessI2cRegisterS(SC0710_I2C_MCU_ADR7_0X33 << 1, NULL, 0, i2c_proxy_rxbuf, i2c_proxy_rxlen, 0);

			RtlCopyMemory(pRxBuf, i2c_proxy_rxbuf, nRxLen);
		}
		DELAY_100NS(100000);

		return is_success;
	}

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
			ZzUtils::TestLoop([&](int ch) -> int {
				int err = 0;

				// LOGD("ch=%d", ch);

				switch(ch) {
				case 'r':
				case 'R':
					OpenUserCtrl();
					break;

				case '1':
					UserCtrl1();
					break;

				case '2':
					UserCtrl2();
					break;

				case '3':
					UserCtrl3();
					break;

				case '4':
					UserCtrl4();
					break;

				case '5':
					UserCtrl5();
					break;

				case '6':
					UserCtrl6();
					break;

				case '7':
					UserCtrl7();
					break;
				}

				return err;
			}, 1000000LL, 1LL);

			err = 0;
		}

		oFreeStack.Flush();

		return err;
	}

	void App::OpenUserCtrl() {
		int err;

		LOGD("%s(%d):...", __FUNCTION__, __LINE__);

		switch(1) { case 1:
			err = oUserCtrl.Open("/dev/qvio0");
			if(err) {
				err = errno;
				LOGE("%s(%d): oUserCtrl.Open() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}
			oFreeStack += [&]() {
				oUserCtrl.Close();
			};
		}
	}

	void App::UserCtrl1() {
		uint32_t R00000008 = oUserCtrl.ReadRegister(0x00000008);

		LOGD("R00000008=%d(0x%X)", R00000008, R00000008);
	}

	void App::UserCtrl2() {
		uint8_t txbuf[ 16 ] = { 0x12, 0x34, 0x57 };
		uint8_t rxbuf[ 16 ] = { 0x00, 0x00, 0x00 };

		if(! oUserCtrl.AccessMcuRegisterS( 0x55, txbuf, 3, rxbuf, 3 )) {
			LOGE("%s(%d): oUserCtrl.AccessMcuRegisterS() failed", __FUNCTION__, __LINE__);
			return;
		}

		LOGD("MCU.RD = %02X.%02X.%02X", rxbuf[0], rxbuf[1], rxbuf[2]);
	}

	void App::UserCtrl3() {
		HDMI20_INTERFACE s_rx_hdmi20_buf;
		memset(&s_rx_hdmi20_buf, 0x00, sizeof(s_rx_hdmi20_buf));

		uint8_t* rxbuf = (uint8_t*)&s_rx_hdmi20_buf.function_name.resolution[0];
		uint8_t txbuf[1] = { (uint8_t)(rxbuf - (uint8_t*)&s_rx_hdmi20_buf) };
		if(! oUserCtrl.AccessSlaveDeviceRegisterS( SC0710_I2C_MCU_ADR7_0X32, txbuf, 1, rxbuf, sizeof(MCU_RESOLUCTION))) {
			LOGE("%s(%d): oUserCtrl.AccessMcuRegisterS() failed", __FUNCTION__, __LINE__);
			return;
		}

		MCU_RESOLUCTION& reso = *(MCU_RESOLUCTION*)rxbuf;
		LOGD("reso={%d,%d,%d,%d,%d,%d,%d}",
			(int)reso.vtotal,
			(int)reso.htotal,
			(int)reso.vactive,
			(int)reso.hactive,
			(int)reso.fps,
			(int)reso.modeflag,
			(int)reso.audiosamplerate,
			(int)reso.colorformat);
	}

	void App::UserCtrl4() {
		uint32_t R000000EC = oUserCtrl.ReadRegister(0x000000EC);
		uint32_t R000000D0 = oUserCtrl.ReadRegister(0x000000D0);

		LOGD("R000000EC=%d(0x%X)", R000000EC, R000000EC);
		LOGD("R000000D0=%d(0x%X)", R000000D0, R000000D0);
	}

	void App::UserCtrl5() {
		uint32_t R000000EC = oUserCtrl.ReadRegister(0x000000EC);
		R000000EC ^= (1 << 4);

		oUserCtrl.WriteRegister(0x000000EC, R000000EC);
		LOGD("R000000EC=%d(0x%X)", R000000EC, R000000EC);
	}

	void App::UserCtrl6() {
		uint32_t R000000EC = oUserCtrl.ReadRegister(0x000000EC);
		R000000EC ^= (1 << 20);

		oUserCtrl.WriteRegister(0x000000EC, R000000EC);
		LOGD("R000000EC=%d(0x%X)", R000000EC, R000000EC);
	}

	void App::UserCtrl7() {
		uint32_t R000000D0 = oUserCtrl.ReadRegister(0x000000D0);
		R000000D0 |= 0x00004110;
		R000000D0 ^= (1 << 0);

		oUserCtrl.WriteRegister(0x000000D0, R000000D0);
		LOGD("R000000D0=%d(0x%X)", R000000D0, R000000D0);
	}
}

using namespace __05_user_ctrl__;
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
