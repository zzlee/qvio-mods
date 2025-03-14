#ifndef __ZZ_CLOCK_H__
#define __ZZ_CLOCK_H__

#include <stdint.h>

struct ZzClock {
	explicit ZzClock();
	~ZzClock();

	int64_t operator()();
};

namespace __zz_clock__ {
	extern ZzClock _clk;
};

#endif // __ZZ_CLOCK_H__
