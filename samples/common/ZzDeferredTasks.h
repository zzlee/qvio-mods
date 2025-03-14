#ifndef __ZZ_DEFERRED_TASKS_H__
#define __ZZ_DEFERRED_TASKS_H__

#include "ZzUtils.h"

#include <thread>
#include <mutex>
#include <deque>
#include <atomic>
#include <memory>
#include <functional>

struct ZzDeferredTasks {
	typedef ZzDeferredTasks self_t;

	explicit ZzDeferredTasks();
	~ZzDeferredTasks();

	int Start();
	void Stop();
	void AddTask(std::function<void()> tsk);

	ZzUtils::FreeStack mFreeStack;
	int mTaskEvent;
	std::thread mThread;
	std::mutex mTaskQMutex;
	std::deque<std::function<void ()> > mTaskQ;

	void Main();
};

#endif // __ZZ_DEFERRED_TASKS_H__