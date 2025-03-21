#include "ZzUtils.h"
#include "ZzLog.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

ZZ_INIT_LOG("ZzUtils");

#include "ZzClock.h"

using namespace __zz_clock__;

namespace ZzUtils {
	FreeStack::FreeStack() {
	}

	FreeStack::~FreeStack() {
		if(! empty()) {
			LOGE("%s(%d): unexpected value, size()=%u", __FUNCTION__, __LINE__, size());
		}
	}

	void FreeStack::Flush() {
		while(! empty()) {
			top()();
			pop();
		}
	}

	ZzStatBitRate::ZzStatBitRate() {
		stats_duration = 1000000LL;
	}

	void ZzStatBitRate::Reset() {
		last_ts = 0;
		cur_ts = 0;
		acc_ticks = 0;
		acc_bits = 0;
		max_bits = 0;
	}

	bool ZzStatBitRate::Log(int64_t bits, int64_t ts) {
		acc_bits += bits;

		if(bits > max_bits) {
			max_bits = bits;
		}

		++acc_ticks;

		int64_t duration = ts - last_ts;
		if(duration > stats_duration) {
			const char* units_name = "bps";
			double factor_den;
			if(acc_bits < 1024LL * 1024LL) {
				units_name = "Kibps";
				factor_den = 1024.0;
			} else if(acc_bits < 1024LL * 1024LL * 1024LL) {
				units_name = "Mibps";
				factor_den = (1024.0 * 1024.0);
			} else {
				units_name = "Gibps";
				factor_den = (1024.0 * 1024.0 * 1024.0);
			}
			double freq = 1000000.0 / duration;

			LOGD("%s: %.2fFPS, %.2f%s, max %.2fKibits .", log_prefix.c_str(), acc_ticks * freq,
				(acc_bits * freq) / factor_den, units_name, max_bits / 1024.0);

			last_ts = ts;
			cur_ts = 0;
			acc_ticks = 0;
			acc_bits = 0;

			return true;
		}

		return false;
	}

	void TestLoop(std::function<int (int)> idle, int64_t dur_num, int64_t dur_den) {
		int err;

		int fd_stdin = 0; // stdin
		int64_t now = _clk();
		int64_t beg = now;
		int64_t nTick = now * dur_den / dur_num - 1;

		LOGW("Wait for test...");
		while(true) {
			fd_set readfds;
			FD_ZERO(&readfds);

			int fd_max = -1;
			if(fd_stdin > fd_max) fd_max = fd_stdin;
			FD_SET(fd_stdin, &readfds);

			struct timeval tval;

			int64_t nInterval = (nTick + 1) * dur_num / dur_den - now;
			if(nInterval < 4000) {
				tval.tv_sec  = 0;
				tval.tv_usec = 4000LL;
			} else {
				tval.tv_sec  = nInterval / dur_num;
				tval.tv_usec = nInterval % dur_num;
			}

			err = select(fd_max + 1, &readfds, NULL, NULL, &tval);
			if (err < 0) {
				LOGE("%s(%d): select() failed! err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			now = _clk();
			nTick++;

			if(err == 0) {
				err = idle(-1);
				if(err) {
					break;
				}

				continue;
			}

			if (FD_ISSET(fd_stdin, &readfds)) {
				int ch = getchar();

				if(ch == 'q')
					break;

				err = idle(ch);
				if(err) {
					break;
				}
			}
		}
		int64_t fini = _clk();
		LOGW("Test done. %.4fs", (fini - beg) / 1000000.0);
	}

	RateCtrl::RateCtrl() {
	}

	void RateCtrl::Start(int64_t t) {
		nDenSecs = den * 1000000LL;
		nTimer = t;
	}

	bool RateCtrl::Next(int64_t t) {
		int64_t nDiff = t - nTimer;
		nTicks = nDiff * num / nDenSecs;
		nCurTime = t;

		return nTicks > 0;
	}

	int64_t RateCtrl::Advance() {
		int64_t nNextTimer = nTimer + (nTicks + 1) * nDenSecs / num;

		nTimer += nTicks * nDenSecs / num;

		return nNextTimer - nCurTime;
	}

	BitRateCtrl::BitRateCtrl() {
	}

	void BitRateCtrl::Start(int64_t t) {
		super_t::Start(t);
		nBits = 0;
	}

	int64_t BitRateCtrl::Advance() {
		int64_t nNextTimer = nTimer + (nTicks + 1) * nDenSecs / num;

		nTimer += nTicks * nDenSecs / num;
		nBits += (nTicks * den * bitrate) / num;

		return nNextTimer - nCurTime;
	}

	bool BitRateCtrl::Consume(int64_t bits) {
		if(bits > nBits)
			return false;

		nBits -= bits;

		return true;
	}

	int64_t BitRateCtrl::Flush() {
		int64_t nRet = nBits;
		nBits = 0;

		return nRet;
	}
}