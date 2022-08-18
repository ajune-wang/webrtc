#pragma once

#include "iohandler.h"
#include "iohandlermanager.h"
#include "tcpcarrier.h"

class TCPConnector: public IOHandler {
public:
	class Observer {
	public:
		virtual ~Observer() {}
		virtual bool OnOutConnection(BaseProtocol *pProtocol) = 0;
	};
private:
	ubnt::abstraction::SocketAddress _targetAddress;
	BaseProtocol *_protocol;
	bool _closeSocket;
	bool _success;
	Observer *_observer;
public:

	TCPConnector(int fd, const ubnt::abstraction::SocketAddress &targetAddress, BaseProtocol *pProtocol, Observer *observer) :
			IOHandler(fd, fd, IOHT_TCP_CONNECTOR), _protocol(pProtocol), _observer(observer) {
		_targetAddress = targetAddress;
		_closeSocket = true;
		_success = false;
	}

	virtual ~TCPConnector() {
		if (!_success) {
			_observer->OnOutConnection(NULL);
		}
		if (_closeSocket) {
			close(_inboundFd);
		}
	}

	virtual bool SignalOutputData() {
		printf("Operation not supported");
		return false;
	}

	virtual bool OnEvent(epoll_event &event) {
		IOHandlerManager::EnqueueForDelete(this);

		if ((event.events & EPOLLERR) != 0) {
			printf("***CONNECT ERROR: Unable to connect to: %s", (const char*) _targetAddress);
			_closeSocket = true;
			return false;
		}

		TCPCarrier *pTCPCarrier = new TCPCarrier(_inboundFd);
		pTCPCarrier->SetProtocol(_protocol->GetFarEndpoint());
		_protocol->GetFarEndpoint()->SetIOHandler(pTCPCarrier);

		if (!_observer->OnOutConnection(_protocol)) {
			printf("Unable to signal protocol created\n");
			delete _protocol;
			_closeSocket = true;
			return false;
		}

		std::cout << "Outbound connection established" << std::endl;

		_success = true;
		_closeSocket = false;
		return true;
	}

	static bool Connect(std::string ip, uint16_t port, uint8_t tos, BaseProtocol *pProtocol, Observer *observer) {

		//create the target address
		ubnt::abstraction::SocketAddress targetAddress(ip, port);
		if (!targetAddress.IsValid()) {
			printf("Invalid target IP/PORT provided");
			observer->OnOutConnection(NULL);
			return false;
		}

		//create the socket
		int fd = socket(targetAddress.GetFamily(), SOCK_STREAM, 0);
		if (SOCKET_IS_INVALID(fd)) {
			int err = errno;
			printf("Unable to create fd: (%d) %s", err, strerror(err));
			observer->OnOutConnection(NULL);
			return 0;
		}

		if ((!setFdOptions(fd, false)) || (!setFdCloseOnExec(fd))) {
			SOCKET_CLOSE(fd);
			printf("Unable to set socket options");
			observer->OnOutConnection(NULL);
			return false;
		}
		if (tos != 0) {
			if (!setIPTOS(fd, tos, targetAddress.IsIPv6())) {
				SOCKET_CLOSE(fd);
				printf("Unable to set TOS");
				observer->OnOutConnection(NULL);
				return false;
			}
		}

		TCPConnector *pTCPConnector = new TCPConnector(fd, targetAddress, pProtocol, observer);

		if (!pTCPConnector->Connect()) {
			IOHandlerManager::EnqueueForDelete(pTCPConnector);
			printf("Unable to connect");
			return false;
		}

		return true;
	}

	bool Connect() {
		if (!IOHandlerManager::EnableWriteData(this)) {
			printf("Unable to enable reading data");
			return false;
		}

		if (connect(_inboundFd, (const sockaddr*) _targetAddress, _targetAddress.GetLength()) != 0) {
			int err = errno;
			if (err != EINPROGRESS) {
				printf("Unable to connect to %s. Error was: (%d) %s", (const char*) _targetAddress, err, strerror(err));
				_closeSocket = true;
				return false;
			}
		}
		_closeSocket = false;
		return true;
	}
};
