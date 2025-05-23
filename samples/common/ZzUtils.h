#ifndef __ZZ_UTILS_H__
#define __ZZ_UTILS_H__

#include <cstdint>
#include <stack>
#include <functional>
#include <string>

#define container_of(ptr, type, member) ({ \
	void* __mptr = (void*)(ptr); \
	((type *)((char *)__mptr - offsetof(type, member))); \
})

#define ZZ_CONCAT_I(N, S) N ## S
#define ZZ_CONCAT(N, S) ZZ_CONCAT_I(N, S)
#define ZZ_GUARD_NAME ZZ_CONCAT(__GUARD, __LINE__)
#define ZZ_ALIGN(x, a) (((x)+(a)-1)&~((a)-1))

namespace ZzUtils {
	struct FreeStack : protected std::stack<std::function<void ()> > {
		typedef FreeStack self_t;
		typedef std::stack<std::function<void ()> > parent_t;

		FreeStack();
		~FreeStack();

		template<class FUNC>
		self_t& operator +=(const FUNC& func) {
			push(func);

			return *this;
		}

		void Flush();
	};

	struct ZzStatBitRate {
		std::string log_prefix;

		int64_t stats_duration;
		int64_t cur_ts;
		int64_t last_ts;

		int acc_ticks;
		int64_t acc_bits;

		int64_t max_bits;

		ZzStatBitRate();

		void Reset();
		bool Log(int64_t bits, int64_t ts);
	};

	struct RateCtrl {
		typedef RateCtrl self_t;

		int num;
		int den;

		explicit RateCtrl();

		void Start(int64_t t);
		bool Next(int64_t t);
		int64_t Advance();

		int64_t nTicks;
		int64_t nCurTime;
		int64_t nDenSecs;
		int64_t nTimer;
	};

	struct BitRateCtrl : public RateCtrl {
		typedef BitRateCtrl self_t;
		typedef RateCtrl super_t;

		int64_t bitrate;

		explicit BitRateCtrl();

		void Start(int64_t t);
		int64_t Advance();
		bool Consume(int64_t bits);
		int64_t Flush();

		int64_t nBits;
	};

	void TestLoop(std::function<int (int)> idle, int64_t dur_num = 1000000LL, int64_t dur_den = 60LL);
}

#endif // __ZZ_UTILS_H__
