#include "common.h"
#include "iohandlermanager.h"
#include "iohandler.h"
#include <sys/epoll.h>

int32_t IOHandlerManager::_eq = 0;
std::map<uint32_t, IOHandler *> IOHandlerManager::_activeIOHandlers;
std::map<uint32_t, IOHandler *> IOHandlerManager::_deadIOHandlers;
struct epoll_event IOHandlerManager::_query[EPOLL_QUERY_SIZE];
std::vector<IOHandlerManagerToken *> IOHandlerManager::_tokensVector1;
std::vector<IOHandlerManagerToken *> IOHandlerManager::_tokensVector2;
std::vector<IOHandlerManagerToken *> *IOHandlerManager::_pAvailableTokens;
std::vector<IOHandlerManagerToken *> *IOHandlerManager::_pRecycledTokens;
int32_t IOHandlerManager::_nextWaitPeriod = 1000;

struct epoll_event IOHandlerManager::_dummy = { 0, { 0 } };

std::map<uint32_t, IOHandler *> & IOHandlerManager::GetActiveHandlers() {
	return _activeIOHandlers;
}

std::map<uint32_t, IOHandler *> & IOHandlerManager::GetDeadHandlers() {
	return _deadIOHandlers;
}

void IOHandlerManager::Initialize() {
	_eq = 0;
	_pAvailableTokens = &_tokensVector1;
	_pRecycledTokens = &_tokensVector2;
	memset(&_dummy, 0, sizeof(_dummy));
}

void IOHandlerManager::Start() {
	_eq = epoll_create(EPOLL_QUERY_SIZE);
}

void IOHandlerManager::SignalShutdown() {
	close(_eq);
}

void IOHandlerManager::ShutdownIOHandlers() {
	FOR_MAP(_activeIOHandlers, uint32_t, IOHandler *, i){
		EnqueueForDelete(MAP_VAL(i));
	}
}

void IOHandlerManager::Shutdown() {
	close(_eq);

	for (uint32_t i = 0; i < _tokensVector1.size(); i++)
		delete _tokensVector1[i];
	_tokensVector1.clear();
	_pAvailableTokens = &_tokensVector1;

	for (uint32_t i = 0; i < _tokensVector2.size(); i++)
		delete _tokensVector2[i];

	_tokensVector2.clear();
	_pRecycledTokens = &_tokensVector2;

	if (_activeIOHandlers.size() != 0 || _deadIOHandlers.size() != 0) {
		printf("Incomplete shutdown!\n");
	}

}

void IOHandlerManager::RegisterIOHandler(IOHandler* pIOHandler) {
	if (MAP_HAS1(_activeIOHandlers, pIOHandler->GetId())) {
		printf("IOHandler already registered\n");
	}
	SetupToken(pIOHandler);
	size_t before = _activeIOHandlers.size();
	_activeIOHandlers[pIOHandler->GetId()] = pIOHandler;

	printf("Handlers count changed: %lu->%lu %s\n", before, before + 1, STR(IOHandler::IOHTToString(pIOHandler->GetType())));
}

void IOHandlerManager::UnRegisterIOHandler(IOHandler *pIOHandler) {
	if (MAP_HAS1(_activeIOHandlers, pIOHandler->GetId())) {
		FreeToken(pIOHandler);
		size_t before = _activeIOHandlers.size();
		_activeIOHandlers.erase(pIOHandler->GetId());
		printf("Handlers count changed: %lu->%lu %s\n", before, before - 1, STR(IOHandler::IOHTToString(pIOHandler->GetType())));
	}
}

bool IOHandlerManager::EnableReadData(IOHandler *pIOHandler) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_ADD, pIOHandler->GetInboundFd(), &evt) != 0) {
		int err = errno;
		printf("Unable to enable read data: (%d) %s\n", err, strerror(err));
		return false;
	}
	return true;
}

bool IOHandlerManager::DisableReadData(IOHandler *pIOHandler, bool ignoreError) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_DEL, pIOHandler->GetInboundFd(), &evt) != 0) {
		if (!ignoreError) {
			int err = errno;
			printf("Unable to disable read data: (%d) %s\n", err, strerror(err));
			return false;
		}
	}
	return true;
}

bool IOHandlerManager::EnableWriteData(IOHandler *pIOHandler) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN | EPOLLOUT;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();

	if (epoll_ctl(_eq, EPOLL_CTL_MOD, pIOHandler->GetOutboundFd(), &evt) != 0) {
		int err = errno;
		if (err == ENOENT) {
			if (epoll_ctl(_eq, EPOLL_CTL_ADD, pIOHandler->GetOutboundFd(), &evt) != 0) {
				err = errno;
				printf("Unable to enable read data: (%d) %s\n", err, strerror(err));
				return false;
			}
		} else {
			printf("Unable to enable read data: (%d) %s\n", err, strerror(err));
			return false;
		}
	}
	return true;
}

bool IOHandlerManager::DisableWriteData(IOHandler *pIOHandler, bool ignoreError) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_MOD, pIOHandler->GetOutboundFd(), &evt) != 0) {
	if (!ignoreError) {
		int err = errno;
		printf("Unable to disable write data: (%d) %s\n", err, strerror(err));
		return false;
	}
	}
	return true;
}

bool IOHandlerManager::EnableAcceptConnections(IOHandler *pIOHandler) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();

	if (epoll_ctl(_eq, EPOLL_CTL_ADD, pIOHandler->GetInboundFd(), &evt) != 0) {
		int err = errno;
		if (err == EEXIST)
			return true;
		printf("    IOHandlerManager::%s Unable to enable accept connections on inbound fd=%d : (%d) %s\n", __func__, pIOHandler->GetInboundFd(), err, strerror(err));
		return false;
	}
	return true;
}

bool IOHandlerManager::DisableAcceptConnections(IOHandler *pIOHandler, bool ignoreError) {
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_DEL, pIOHandler->GetInboundFd(), &evt) != 0) {
		if (!ignoreError) {
			int err = errno;
			printf("Unable to disable accept connections: (%d) %s\n", err, strerror(err));
			return false;
		}
	}
	return true;
}

bool IOHandlerManager::EnableTimer(IOHandler *pIOHandler, uint32_t seconds) {

	itimerspec tmp;
	itimerspec dummy;
	memset(&tmp, 0, sizeof (tmp));
	tmp.it_interval.tv_nsec = 0;
	tmp.it_interval.tv_sec = seconds;
	tmp.it_value.tv_nsec = 0;
	tmp.it_value.tv_sec = seconds;
	if (timerfd_settime(pIOHandler->GetInboundFd(), 0, &tmp, &dummy) != 0) {
		int err = errno;
		printf("timerfd_settime failed with error (%d) %s\n", err, strerror(err));
		return false;
	}
	struct epoll_event evt = {0,
		{0}};
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_ADD, pIOHandler->GetInboundFd(), &evt) != 0) {
		int err = errno;
		printf("Unable to enable read data: (%d) %s\n", err, strerror(err));
		return false;
	}
	return true;
}

std::string dumpTimerStruct(itimerspec &ts) {
	return format("it_interval\n\ttv_sec: %lu\n\ttv_nsec: %ld\nit_value\n\ttv_sec: %lu\n\ttv_nsec: %ld",
			ts.it_interval.tv_sec,
			ts.it_interval.tv_nsec,
			ts.it_value.tv_sec,
			ts.it_value.tv_nsec);
}

bool IOHandlerManager::EnableHighGranularityTimer(IOHandler *pIOHandler, uint32_t milliseconds) {

	itimerspec tmp;
	itimerspec dummy;
	memset(&tmp, 0, sizeof(tmp));
	tmp.it_interval.tv_nsec = (milliseconds % 1000) * 1000000;
	tmp.it_interval.tv_sec = milliseconds / 1000;
	tmp.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
	tmp.it_value.tv_sec = milliseconds / 1000;
	//	ASSERT("milliseconds: %" PRIu32 "\n%s",
	//			milliseconds,
	//			STR(dumpTimerStruct(tmp)));
	if (timerfd_settime(pIOHandler->GetInboundFd(), 0, &tmp, &dummy) != 0) {
		int err = errno;
		printf("timerfd_settime failed with error (%d) %s", err, strerror(err));
		return false;
	}
	struct epoll_event evt = { 0, { 0 } };
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_ADD, pIOHandler->GetInboundFd(), &evt) != 0) {
		int err = errno;
		printf("Unable to enable read data: (%d) %s", err, strerror(err));
		return false;
	}
	return true;
}

bool IOHandlerManager::DisableTimer(IOHandler *pIOHandler, bool ignoreError) {
	itimerspec tmp;
	itimerspec dummy;
	memset(&tmp, 0, sizeof (tmp));
	timerfd_settime(pIOHandler->GetInboundFd(), 0, &tmp, &dummy);
	struct epoll_event evt = {0,
		{0}};
	evt.events = EPOLLIN;
	evt.data.ptr = pIOHandler->GetIOHandlerManagerToken();
	if (epoll_ctl(_eq, EPOLL_CTL_DEL, pIOHandler->GetInboundFd(), &evt) != 0) {
		if (!ignoreError) {
			int err = errno;
			printf("Unable to disable read data: (%d) %s", err, strerror(err));
			return false;
		}
	}
	return true;
}

void IOHandlerManager::EnqueueForDelete(IOHandler *pIOHandler) {
	DisableWriteData(pIOHandler, true);
	DisableAcceptConnections(pIOHandler, true);
	DisableReadData(pIOHandler, true);
	DisableTimer(pIOHandler, true);
	if (!MAP_HAS1(_deadIOHandlers, pIOHandler->GetId())){
		_deadIOHandlers[pIOHandler->GetId()] = pIOHandler;
	}
}

uint32_t IOHandlerManager::DeleteDeadHandlers() {
	uint32_t result = 0;
	while (_deadIOHandlers.size() > 0) {
		IOHandler *pIOHandler = MAP_VAL(_deadIOHandlers.begin());
		_deadIOHandlers.erase(pIOHandler->GetId());
		delete pIOHandler;
		result++;
	}
	return result;
}

bool IOHandlerManager::Pulse() {
	int32_t eventsCount = 0;

	if ((eventsCount = epoll_wait(_eq, _query, EPOLL_QUERY_SIZE, -1)) < 0) {
		int err = errno;
		if (err == EINTR)
			return true;
		printf("Unable to execute epoll_wait: (%d) %s\n", err, strerror(err));
		return false;
	}

	for (int32_t i = 0; i < eventsCount; i++) {
		//1. Get the token
		IOHandlerManagerToken *pToken = (IOHandlerManagerToken *) _query[i].data.ptr;

		//2. Test the fd
		if ((_query[i].events & EPOLLERR) != 0) {
			if (pToken->validPayload) {
				//uint64_t start = GetTimeMillis();

				if ((_query[i].events & EPOLLHUP) != 0) {
					//printf("***Event handler HUP: %p", (IOHandler *) pToken->pPayload);
					((IOHandler *) pToken->pPayload)->OnEvent(_query[i]);
				}
				//				else {
				//					printf("***Event handler ERR: %p", (IOHandler *) pToken->pPayload);
				//				}
				IOHandlerManager::EnqueueForDelete((IOHandler *) pToken->pPayload);
			}
			continue;
		}

		//3. Do the damage
		if (pToken->validPayload) {
			if (!((IOHandler *) pToken->pPayload)->OnEvent(_query[i])) {
				EnqueueForDelete((IOHandler *) pToken->pPayload);
			}
		}
		else {
			printf("Invalid token\n");
		}
	}

	if (_tokensVector1.size() > _tokensVector2.size()) {
		_pAvailableTokens = &_tokensVector1;
		_pRecycledTokens = &_tokensVector2;
	} else {
		_pAvailableTokens = &_tokensVector2;
		_pRecycledTokens = &_tokensVector1;
	}
	return true;
}

void IOHandlerManager::SetupToken(IOHandler *pIOHandler) {
	IOHandlerManagerToken *pResult = NULL;
	if (_pAvailableTokens->size() == 0) {
		pResult = new IOHandlerManagerToken();
	} else {
		pResult = (*_pAvailableTokens)[0];
		_pAvailableTokens->erase(_pAvailableTokens->begin());
	}
	pResult->pPayload = pIOHandler;
	pResult->validPayload = true;
	pIOHandler->SetIOHandlerManagerToken(pResult);
}

void IOHandlerManager::FreeToken(IOHandler *pIOHandler) {
	IOHandlerManagerToken *pToken = pIOHandler->GetIOHandlerManagerToken();
	pIOHandler->SetIOHandlerManagerToken(NULL);
	pToken->pPayload = NULL;
	pToken->validPayload = false;
	ADD_VECTOR_END((*_pRecycledTokens), pToken);
}
