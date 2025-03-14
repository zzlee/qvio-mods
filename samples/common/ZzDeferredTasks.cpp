#include "ZzDeferredTasks.h"
#include "ZzLog.h"

#include <unistd.h>
#include <sys/eventfd.h>

ZZ_INIT_LOG("ZzDeferredTasks");

ZzDeferredTasks::ZzDeferredTasks() {
}

ZzDeferredTasks::~ZzDeferredTasks() {
}

int ZzDeferredTasks::Start() {
	int err;

	switch(1) { case 1:
		err = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
		if(err < 0) {
			err = errno;
			LOGE("%s(%d): eventfd() failed, err=%d", __FUNCTION__, __LINE__, err);
			break;
		}
		mTaskEvent = err;
		mFreeStack += [&]() {
			int err;

			err = close(mTaskEvent);
			if(err) {
				LOGE("%s(%d): close() failed, err=%d", __FUNCTION__, __LINE__, err);
			}
		};
		err = 0;

		std::thread t(std::bind(&self_t::Main, this));
		mThread.swap(t);
		mFreeStack += [&]() {
			int err;

			switch(1) { case 1:
				// notify mThread to stop
				int64_t inc = 1;
				err = write(mTaskEvent, &inc, sizeof(inc));
				if(err == -1) {
					err = errno;
					LOGE("%s(%d): write() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				if(err != sizeof(inc)) {
					err = errno;
					LOGE("%s(%d): write() failed, err=%d", __FUNCTION__, __LINE__, err);
					break;
				}

				mThread.join();
			}
		};
	}

	if(err) {
		mFreeStack.Flush();
	}

	return err;
}

void ZzDeferredTasks::Stop() {
	mFreeStack.Flush();
}

void ZzDeferredTasks::AddTask(std::function<void()> tsk) {
	int err;
	std::unique_lock<std::mutex> lck (mTaskQMutex, std::defer_lock);

	lck.lock();

	switch(1) { case 1:
		mTaskQ.push_back(tsk);

		// wake mTaskQ up
		int64_t inc = 1;
		err = write(mTaskEvent, &inc, sizeof(inc));
		if(err == -1) {
			err = errno;
			LOGE("%s(%d): write() failed, err=%d", __FUNCTION__, __LINE__, err);
			break;
		}

		if(err != sizeof(inc)) {
			err = EFAULT;
			LOGE("%s(%d): write() failed, err=%d", __FUNCTION__, __LINE__, err);
			break;
		}
	}

	lck.unlock();
}

void ZzDeferredTasks::Main() {
	int err;

	LOGD("%s(%d):+++", __FUNCTION__, __LINE__);

	while(true) {
		fd_set readfds;
		FD_ZERO(&readfds);

		int fd_max = -1;
		if(mTaskEvent > fd_max) fd_max = mTaskEvent;
		FD_SET(mTaskEvent, &readfds);

		err = select(fd_max + 1, &readfds, NULL, NULL, NULL);
		if (err < 0) {
			LOGE("%s(%d): select() failed! err = %d", __FUNCTION__, __LINE__, err);
			break;
		}

		if (FD_ISSET(mTaskEvent, &readfds)) {
			int64_t inc;
			err = read(mTaskEvent, &inc, sizeof(inc));
			if(err == -1) {
				err = errno;
				LOGE("%s(%d): read() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			if(err != sizeof(inc)) {
				err = errno;
				LOGE("%s(%d): read() failed, err=%d", __FUNCTION__, __LINE__, err);
				break;
			}

			std::unique_lock<std::mutex> lck (mTaskQMutex, std::defer_lock);
			std::function<void ()> tsk;

			lck.lock();
			switch(1) { case 1:
				if(mTaskQ.empty()) {
					// LOGW("%s(%d): unexpected, mTaskQ.empty(), stopping...", __FUNCTION__, __LINE__);
					break;
				}

				tsk = mTaskQ.front();
				mTaskQ.pop_front();
			}
			lck.unlock();

			if(! tsk)
				break;

			tsk();
		}
	}

	LOGD("%s(%d):---", __FUNCTION__, __LINE__);
}
