#pragma once

#include "common.h"
#include "iohandler.h"
#include "socketaddress.h"
#include "tcpprotocol.h"

class TCPAcceptor: public IOHandler {
public:
	class Observer {
	public:
		virtual ~Observer() {}
		virtual void OnInConnection(BaseProtocol *pProtocol) {
		}
	};
private:
	ubnt::abstraction::SocketAddress _address;
	bool _enabled;
	uint32_t _acceptedCount;
	uint32_t _droppedCount;
	uint8_t _tos;
	Observer *_observer;
	TCPProtocol::Observer *_tcpobserver;
	uint32_t _messageLength;
public:
	bool _log;
	TCPAcceptor(const ubnt::abstraction::SocketAddress &address, uint8_t tos, Observer *observer, TCPProtocol::Observer* tcpobserver);
	virtual ~TCPAcceptor();
	const ubnt::abstraction::SocketAddress& GetBindAddress() const;
	bool Bind();
	bool StartAccept();
	virtual bool SignalOutputData();
	virtual bool OnEvent(struct epoll_event &event);
	virtual bool OnConnectionAvailable(struct epoll_event &event);
	bool Accept();
	bool Drop();
	std::vector<uint64_t>& GetProtocolChain();
	bool Enable();
	void Enable(bool enabled);
private:
	bool IsAlive();
};
