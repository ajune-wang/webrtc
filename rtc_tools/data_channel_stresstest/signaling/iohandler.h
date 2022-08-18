#pragma once

#include "common.h"
#include "iohandlermanagertoken.h"
#include "iohandlertype.h"
#include "baseprotocol.h"

class BaseProtocol;

class IOHandler {
protected:
	static uint32_t _idGenerator;
	uint32_t _id;
	int _inboundFd;
	int _outboundFd;
	BaseProtocol *_pProtocol;
	IOHandlerType _type;

private:
	IOHandlerManagerToken *_pToken;
public:

	bool _log;
	std::string _name;
	uint64_t _lastMs;
	uint64_t _lastMs2;

	IOHandler(int inboundFd, int outboundFd, IOHandlerType type);
	virtual ~IOHandler();
	void SetIOHandlerManagerToken(IOHandlerManagerToken *pToken);

	IOHandlerManagerToken * GetIOHandlerManagerToken();

	uint32_t GetId();

	int GetInboundFd();
	int GetOutboundFd();


	void SetProtocol(BaseProtocol *pPotocol);
	BaseProtocol *GetProtocol();

	IOHandlerType GetType();

	virtual bool SignalOutputData() = 0;
	virtual bool OnEvent(struct epoll_event &event) = 0;

	static std::string IOHTToString(IOHandlerType type);
};
