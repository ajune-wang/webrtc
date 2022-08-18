#pragma once

#include "iohandler.h"

class IOTimer: public IOHandler {
private:
	uint64_t _count;
public:
	IOTimer();
	virtual ~IOTimer();

	virtual bool SignalOutputData();
	virtual bool OnEvent(struct epoll_event &eventWrapper);
	bool EnqueueForTimeEvent(uint32_t seconds);
	bool EnqueueForHighGranularityTimeEvent(uint32_t milliseconds);
};
