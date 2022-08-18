#pragma once

#include "iohandler.h"
#include "socketaddress.h"

class TCPCarrier: public IOHandler {
private:
	long _writeDataEnabled;
	bool _enableWriteDataCalled;
	ubnt::abstraction::SocketAddress _nearAddress;
	ubnt::abstraction::SocketAddress _farAddress;
	int32_t _sendBufferSize;
	int32_t _recvBufferSize;
	uint64_t _rx;
	uint64_t _tx;
	int32_t _ioAmount;
	int _lastRecvError;
	int _lastSendError;

public:
	uint64_t _writeStart;

	TCPCarrier(int fd);
	virtual ~TCPCarrier();
	virtual bool OnEvent(struct epoll_event &event);
	virtual bool SignalOutputData();

	const ubnt::abstraction::SocketAddress &GetFarAddress() const;
	const ubnt::abstraction::SocketAddress &GetNearAddress() const;
private:
	bool DetectAddresses();
};
