#include "iotimer.h"
#include "iohandlermanager.h"
#include "baseprotocol.h"

#include <sys/timerfd.h>

IOTimer::IOTimer() :
		IOHandler(0, 0, IOHT_TIMER) {
	_count = 0;
	_inboundFd = _outboundFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (_inboundFd < 0) {
		int err = errno;
		printf("timerfd_create failed with error (%d) %s\n", err, strerror(err));
	}
}

IOTimer::~IOTimer() {
	IOHandlerManager::DisableTimer(this, true);
	close(_inboundFd);
}

bool IOTimer::SignalOutputData() {
	printf("Operation not supported\n");
	return false;
}

bool IOTimer::OnEvent(struct epoll_event&/*ignored*/) {
	if (read(_inboundFd, &_count, 8) != 8) {
		printf("Timer failed!\n");
		return false;
	}
	if (!_pProtocol->IsEnqueueForDelete()) {
		if (!_pProtocol->TimePeriodElapsed()) {
			printf("Unable to handle TimeElapsed event\n");
			IOHandlerManager::EnqueueForDelete(this);
			return false;
		}
	}
	return true;
}

bool IOTimer::EnqueueForTimeEvent(uint32_t seconds) {
	return IOHandlerManager::EnableTimer(this, seconds);
}

bool IOTimer::EnqueueForHighGranularityTimeEvent(uint32_t milliseconds) {
	return IOHandlerManager::EnableHighGranularityTimer(this, milliseconds);
}
