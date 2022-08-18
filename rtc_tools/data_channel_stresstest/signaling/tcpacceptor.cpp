#include "common.h"
#include "tcpacceptor.h"
#include "iohandlermanager.h"
#include "tcpcarrier.h"
#include "tcpprotocol.h"

TCPAcceptor::TCPAcceptor(const ubnt::abstraction::SocketAddress &address, uint8_t tos, Observer *observer, TCPProtocol::Observer* tcpobserver) :
		IOHandler(0, 0, IOHT_ACCEPTOR), _observer(observer), _tcpobserver(tcpobserver) {
	_address = address;
	_enabled = false;
	_acceptedCount = 0;
	_droppedCount = 0;
	_tos = tos;
	_log = false;
}

TCPAcceptor::~TCPAcceptor() {
	close(_inboundFd);
}

const ubnt::abstraction::SocketAddress& TCPAcceptor::GetBindAddress() const {
	return _address;
}

bool TCPAcceptor::Bind() {

	printf("#-> TCPAcceptor::%s\n", __func__);

	//check the address
	if (!_address.IsValid()) {
		printf("Invalid address provided");
		return false;
	}

	//create the socket
	_inboundFd = _outboundFd = socket(_address.GetFamily(), SOCK_STREAM, 0);

	printf("    TCPAcceptor::%s  _inboundFd = _outboundFd = %d\n", __func__, _inboundFd);

	if (SOCKET_IS_INVALID(_inboundFd)) {
		int err = errno;
		printf("Unable to create socket: (%d) %s", err, strerror(err));
		return false;
	}

	//set the options on it
	if (!setFdOptions(_inboundFd, false)) {
		printf("Unable to set socket options");
		return false;
	}

	//bind it
	if (::bind(_inboundFd, (const sockaddr*) _address, _address.GetLength()) != 0) {
		int err = errno;
		printf("Unable to bind on address: tcp://%s; Error was: (%d) %s", (const char*) _address, err, strerror(err));
		return false;
	}

	//get the port number if it was not specified
	if (_address.GetPort() == 0) {
		sockaddr_storage temp;
		socklen_t len = sizeof(temp);
		if (getsockname(_inboundFd, (sockaddr*) &temp, &len) != 0) {
			printf("Unable to extract the random port");
			return false;
		}
		_address = &temp;
	}

	//enter listening mode
	if (listen(_inboundFd, 100) != 0) {
		printf("Unable to put the socket in listening mode");
		return false;
	}

	//set it as enabled
	_enabled = true;

	printf("<-# TCPAcceptor::%s\n", __func__);
	//done
	return true;
}

bool TCPAcceptor::StartAccept() {
	return IOHandlerManager::EnableAcceptConnections(this);
}

bool TCPAcceptor::SignalOutputData() {
	printf("Operation not supported");
	return false;
}

bool TCPAcceptor::OnEvent(struct epoll_event &event) {
	if (!OnConnectionAvailable(event))
		return IsAlive();
	else
		return true;
}

bool TCPAcceptor::OnConnectionAvailable(struct epoll_event &event) {
	//if (_pApplication == NULL)
	return Accept();
	//return _pApplication->AcceptTCPConnection(this);
}

bool TCPAcceptor::Accept() {
	sockaddr_storage address;
	memset(&address, 0, sizeof(address));
	socklen_t len = sizeof(address);
	int fd;

	fd = accept(_inboundFd, (sockaddr*) &address, &len);

	if (SOCKET_IS_INVALID(fd) || (!setFdCloseOnExec(fd))) {
		int err = errno;
		printf("Unable to accept client connection: (%d) %s", err, strerror(err));
		return false;
	}

	if (!_enabled) {
		SOCKET_CLOSE(fd);
		_droppedCount++;
		ubnt::abstraction::SocketAddress temp(&address);
		printf("Acceptor is not enabled. Client dropped: %s -> %s", (const char*) temp, (const char*) _address);
		return true;
	}

	if (!setFdOptions(fd, false)) {
		printf("Unable to set socket options");
		SOCKET_CLOSE(fd);
		return false;
	}

	//set the TOS on socket
	if (_tos != 0) {
		if (!setIPTOS(fd, _tos, _address.IsIPv6())) {
			printf("Unable to set TOS");
			SOCKET_CLOSE(fd);
			return false;
		}
	}

	//4. Create the chain
	BaseProtocol *pProtocol = new TCPProtocol("s", _tcpobserver);
	if (pProtocol == NULL) {
		printf("Unable to create protocol chain");
		SOCKET_CLOSE(fd);
		return false;
	}

	//5. Create the carrier and bind it
	TCPCarrier *pTCPCarrier = new TCPCarrier(fd);

	pTCPCarrier->SetProtocol(pProtocol->GetFarEndpoint());
	pProtocol->GetFarEndpoint()->SetIOHandler(pTCPCarrier);

	if (pProtocol->GetNearEndpoint()->GetOutputBuffer() != NULL)
		pProtocol->GetNearEndpoint()->EnqueueForOutbound();

	if (_observer)
		_observer->OnInConnection(pProtocol);

	_acceptedCount++;

	printf("Inbound connection accepted\n");

	return true;
}

bool TCPAcceptor::Drop() {
	sockaddr_storage address;
	memset(&address, 0, sizeof(address));
	socklen_t len = sizeof(address);

	int fd = accept(_inboundFd, (sockaddr*) &address, &len);
	if (SOCKET_IS_INVALID(fd) || (!setFdCloseOnExec(fd))) {
		int err = errno;
		if (err != EWOULDBLOCK)
			printf("Accept failed. Error code was: (%d) %s", err, strerror(err));
		return false;
	}

	SOCKET_CLOSE(fd);
	_droppedCount++;

	ubnt::abstraction::SocketAddress temp(&address);

	printf("Client explicitly dropped: %s -> %s\n", (const char*) temp, (const char*) _address);
	return true;
}

bool TCPAcceptor::Enable() {
	return _enabled;
}

void TCPAcceptor::Enable(bool enabled) {
	_enabled = enabled;
}

bool TCPAcceptor::IsAlive() {
	return true;
}
