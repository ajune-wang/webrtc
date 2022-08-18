#include "common.h"
#include "tcpcarrier.h"
#include "iohandlermanager.h"
#include "iobuffer.h"

#define ENABLE_WRITE_DATA \
if (!_writeDataEnabled) { \
    _writeDataEnabled++; \
    _writeStart = GetTimeMillis(); \
    IOHandlerManager::EnableWriteData(this); \
} \
_enableWriteDataCalled=true;

#define DISABLE_WRITE_DATA \
if (_writeDataEnabled) { \
	_enableWriteDataCalled=false; \
	if(!_enableWriteDataCalled) { \
		if(_pProtocol->GetOutputBuffer()==NULL) {\
			_writeDataEnabled = 0; \
			IOHandlerManager::DisableWriteData(this); \
		} \
	} \
}

TCPCarrier::TCPCarrier(int fd) :
		IOHandler(fd, fd, IOHT_TCP_CARRIER) {
	IOHandlerManager::EnableReadData(this);
	_writeDataEnabled = 0;
	_enableWriteDataCalled = false;
	_sendBufferSize = 1024 * 1024 * 8;
	_recvBufferSize = 1024 * 256;
	DetectAddresses();
	_rx = 0;
	_tx = 0;
	_ioAmount = 0;
	_lastRecvError = 0;
	_lastSendError = 0;
}

TCPCarrier::~TCPCarrier() {
	SOCKET_CLOSE(_inboundFd);
}

bool TCPCarrier::OnEvent(struct epoll_event &event) {
	if ((event.events & EPOLLIN) != 0) {

		IOBuffer *pInputBuffer = _pProtocol->GetInputBuffer();

		if (!pInputBuffer->ReadFromTCPFd(_inboundFd, _recvBufferSize, _ioAmount, _lastRecvError)) {
			std::cout
					<< format("Unable to read data from connection: %s. Error was (%d): %s", (_pProtocol != NULL) ? STR(*_pProtocol) : "",
							_lastRecvError, strerror(_lastRecvError)) << std::endl;

			_pProtocol->OnDisconnect();

			return false;
		}
		_rx += _ioAmount;

		if (!_pProtocol->SignalInputData(_ioAmount)) {
			std::cout << format("%s failed to process data.\n", (_pProtocol != NULL) ? STR(*_pProtocol) : "unknown protocol") << std::endl;
			return false;
		}
		return true;

	}

	if ((event.events & EPOLLOUT) != 0) {

		IOBuffer *pOutputBuffer = NULL;

		if ((pOutputBuffer = _pProtocol->GetOutputBuffer()) != NULL) {
			if (!pOutputBuffer->WriteToTCPFd(_inboundFd, _sendBufferSize, _ioAmount, _lastSendError)) {
				printf("Unable to write data on connection: %s. Error was (%d): %s\n", (_pProtocol != NULL) ? STR(*_pProtocol) : "", _lastSendError,
						strerror(_lastSendError));
				IOHandlerManager::EnqueueForDelete(this);
				return false;
			}
			_tx += _ioAmount;
			if (GETAVAILABLEBYTESCOUNT(*pOutputBuffer) == 0) {
				DISABLE_WRITE_DATA;
			}
		} else {
			DISABLE_WRITE_DATA;
		}
		return true;
	}
	return true;
}

bool TCPCarrier::SignalOutputData() {
	ENABLE_WRITE_DATA;
	return true;
}

const ubnt::abstraction::SocketAddress& TCPCarrier::GetFarAddress() const {
	return _farAddress;
}

const ubnt::abstraction::SocketAddress& TCPCarrier::GetNearAddress() const {
	return _nearAddress;
}

bool TCPCarrier::DetectAddresses() {
	if (_nearAddress.IsValid() && _farAddress.IsValid())
		return true;

	//get the far address
	sockaddr_storage temp;
	socklen_t tempLen = sizeof(temp);
	if (getpeername(_inboundFd, (sockaddr*) &temp, &tempLen) != 0) {
		std::cout << "Unable to get far address";
		return false;
	}
	_farAddress = &temp;

	//get the near address
	tempLen = sizeof(temp);
	if (getsockname(_inboundFd, (sockaddr*) &temp, &tempLen) != 0) {
		std::cout << "Unable to get near address";
		return false;
	}
	_nearAddress = &temp;

	//done
	return true;
}
