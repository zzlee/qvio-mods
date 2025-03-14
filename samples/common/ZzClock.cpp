#include "ZzClock.h"
#include "ZzLog.h"
#include <time.h>
#include <sys/time.h>

ZZ_INIT_LOG("ZzClock");

namespace __zz_clock__ {
	ZzClock _clk;

	inline int64_t GetTime() {
		int64_t ret;

		struct timespec cur_time;
		clock_gettime(CLOCK_MONOTONIC, &cur_time);

		ret = (int64_t)cur_time.tv_sec * 1000000LL + (int64_t)cur_time.tv_nsec / 1000LL;

		return ret;
	}
}

using namespace __zz_clock__;

ZzClock::ZzClock() {
}

ZzClock::~ZzClock() {
}

int64_t ZzClock::operator()() {
	return GetTime();
}
