#pragma once

#include "common.h"
#include "iohandlermanagertoken.h"

class IOHandler;

#define EPOLL_QUERY_SIZE 1024

class IOHandlerManager {
private:
	static int32_t _eq;
	static std::map<uint32_t, IOHandler *> _activeIOHandlers;
	static std::map<uint32_t, IOHandler *> _deadIOHandlers;
	static struct epoll_event _query[EPOLL_QUERY_SIZE];
	static std::vector<IOHandlerManagerToken *> _tokensVector1;
	static std::vector<IOHandlerManagerToken *> _tokensVector2;
	static std::vector<IOHandlerManagerToken *> *_pAvailableTokens;
	static std::vector<IOHandlerManagerToken *> *_pRecycledTokens;
	static int32_t _nextWaitPeriod;
public:
	static std::map<uint32_t, IOHandler *> & GetActiveHandlers();
	static std::map<uint32_t, IOHandler *> & GetDeadHandlers();
	static struct epoll_event _dummy;

	static void Initialize();
	static void Start();
	static void SignalShutdown();
	static void ShutdownIOHandlers();
	static void Shutdown();
	static void RegisterIOHandler(IOHandler *pIOHandler);
	static void UnRegisterIOHandler(IOHandler *pIOHandler);

	static bool EnableReadData(IOHandler *pIOHandler);
	static bool DisableReadData(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableWriteData(IOHandler *pIOHandler);
	static bool DisableWriteData(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableAcceptConnections(IOHandler *pIOHandler);
	static bool DisableAcceptConnections(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableTimer(IOHandler *pIOHandler, uint32_t seconds);
	static bool EnableHighGranularityTimer(IOHandler *pIOHandler, uint32_t milliseconds);
	static bool DisableTimer(IOHandler *pIOHandler, bool ignoreError = false);
	static void EnqueueForDelete(IOHandler *pIOHandler);
	static uint32_t DeleteDeadHandlers();
	static bool Pulse();
private:
	static void SetupToken(IOHandler *pIOHandler);
	static void FreeToken(IOHandler *pIOHandler);

};
