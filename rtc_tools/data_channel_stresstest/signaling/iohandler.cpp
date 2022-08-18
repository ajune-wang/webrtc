#include "iohandler.h"
#include "iohandlermanager.h"

uint32_t IOHandler::_idGenerator = 0;

IOHandler::IOHandler(int inboundFd, int outboundFd, IOHandlerType type) {
	_pProtocol = NULL;
	_type = type;
	_id = ++_idGenerator;
	_inboundFd = inboundFd;
	_outboundFd = outboundFd;
	_pToken = NULL;
	IOHandlerManager::RegisterIOHandler(this);

	_log = false;
	_lastMs = GetTimeMillis();
}

IOHandler::~IOHandler() {
	if (_pProtocol != NULL) {
		_pProtocol->SetIOHandler(NULL);
		_pProtocol->EnqueueForDelete();
		_pProtocol = NULL;
	}
	IOHandlerManager::UnRegisterIOHandler(this);
}

void IOHandler::SetIOHandlerManagerToken(IOHandlerManagerToken *pToken) {
	_pToken = pToken;
}

IOHandlerManagerToken * IOHandler::GetIOHandlerManagerToken() {
	return _pToken;
}

uint32_t IOHandler::GetId() {
	return _id;
}

void IOHandler::SetProtocol(BaseProtocol *pPotocol) {
	_pProtocol = pPotocol;
}

BaseProtocol *IOHandler::GetProtocol() {
	return _pProtocol;
}


int IOHandler::GetInboundFd() {
	return _inboundFd;
}

int IOHandler::GetOutboundFd() {
	return _outboundFd;
}

IOHandlerType IOHandler::GetType() {
	return _type;
}

std::string IOHandler::IOHTToString(IOHandlerType type) {
	switch (type) {
	case IOHT_ACCEPTOR:
		return "IOHT_ACCEPTOR";
	case IOHT_TCP_CARRIER:
		return "IOHT_TCP_CARRIER";
	case IOHT_UDP_CARRIER:
		return "IOHT_UDP_CARRIER";
	case IOHT_TCP_CONNECTOR:
		return "IOHT_TCP_CONNECTOR";
	case IOHT_TIMER:
		return "IOHT_TIMER";
	case IOHT_INBOUNDNAMEDPIPE_CARRIER:
		return "IOHT_INBOUNDNAMEDPIPE_CARRIER";
	default:
		return "unknown";
	}
}

