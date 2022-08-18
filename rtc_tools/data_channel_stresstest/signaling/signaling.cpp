#include <thread>
#include <chrono>
#include "rtc_tools/data_channel_stresstest/signaling/signaling.h"
#include <unistd.h>

#include "iohandler.h"
#include "iohandlermanager.h"
#include "iohandlermanagertoken.h"

#include "tcpacceptor.h"
#include "tcpcarrier.h"
#include "tcpconnector.h"
#include "tcpprotocol.h"

signaling::~signaling() {
}

bool signaling::Init() {
	std::cout << "    Initialize I/O handlers manager" << std::endl;
	IOHandlerManager::Initialize();
	std::cout << "    Start I/O handlers manager" << std::endl;
	IOHandlerManager::Start();

	if (_offerer) {
		std::cout << "    Connector" << std::endl;

		BaseProtocol *pProtocol = new TCPProtocol("c", this);
		if (pProtocol == NULL) {
			std::cout << "Unable to create protocol chain" << std::endl;
			return false;
		}
		if (!TCPConnector::Connect(_address, _port, SOCKET_TOS_DSCP_EF, pProtocol, this)) {
			std::cout << format("Unable to connect to %s:%hu\n", STR(_address), _port) << std::endl;
			delete pProtocol;
			return false;
		}
		_outconnections.insert(std::make_pair(pProtocol->GetId(), pProtocol));

	} else {

		std::cout << "    Acceptor" << std::endl;

		ubnt::abstraction::SocketAddress bindAddress(_address, _port);
		if (!bindAddress.IsValid()) {
			std::cout << format("Unable to bind on %s:%d\n", _address.c_str(), _port) << std::endl;
			return false;
		}

		_acceptors = new TCPAcceptor(bindAddress, SOCKET_TOS_DSCP_EF, this, this);
		if (!_acceptors->Bind()) {
			std::cout << format("Unable to fire up acceptor to: %s", (const char*) bindAddress) << std::endl;
			return false;
		}
		if (!_acceptors->StartAccept()) {
			printf("Unable to start acceptor\n");
			return false;
		}

	}
	return true;
}

bool signaling::CleanupDeadProtocols() {

	for (auto it = _inconnections.begin(); it != _inconnections.end();) {
		if (it->second->IsEnqueueForDelete()) {
			_inconnections.erase(it++);
		} else {
			++it;
		}
	}
	for (auto it = _outconnections.begin(); it != _outconnections.end();) {
		if (it->second->IsEnqueueForDelete()) {
			_inconnections.erase(it++);
		} else {
			++it;
		}
	}

	return true;
}

// Outgoing connection connected
bool signaling::OnOutConnection(BaseProtocol *pProtocol) {
	if (pProtocol == NULL) {
		std::cout << format("Connection failed");
		return true;
	}

	//pProtocol->EnqueueForHighGranularityTimeEvent(33);

	if (_on_connect) {
		_on_connect(pProtocol->GetId());
	}

	std::cout << format("Connection success") << std::endl;
	return true;
}

// Incoming connection accepted
void signaling::OnInConnection(BaseProtocol *pProtocol) {
	_inconnections.insert(std::make_pair(pProtocol->GetId(), pProtocol));

	if (_on_connect) {
		_on_connect(pProtocol->GetId());
	}
}

bool signaling::OnMessage(BaseProtocol *pProtocol, uint8_t *buffer, uint32_t size) {
	std::string msg((char*) buffer, size);

	std::cout << "signaling::OnMessage() " << msg << std::endl;

	if (_on_message) {
		_on_message(pProtocol->GetId(), msg);
	}
	return true;
}

void signaling::OnDisconnect(BaseProtocol *pProtocol) {
	if (_on_disconnect) {
		_on_disconnect(pProtocol->GetId());
	}
}

bool signaling::send(int id, const std::string msg) {
	if (_offerer) {
		_outconnections[id]->SendMessage(msg);
	} else {
		_inconnections[id]->SendMessage(msg);
	}
	return true;
}

bool signaling::Run() {
	std::cout << "#-> signaling::" << __func__ << std::endl;
	while (1) {
		if (!IOHandlerManager::Pulse())
			break;
		IOHandlerManager::DeleteDeadHandlers();
		CleanupDeadProtocols();
	}

	std::cout << "<-# signaling::" << __func__ << " Exiting ..." << std::endl;
	return true;
}

