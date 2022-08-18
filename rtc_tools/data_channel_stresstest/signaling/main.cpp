#include "iohandler.h"
#include "iohandlermanager.h"
#include "iohandlermanagertoken.h"

#include "tcpacceptor.h"
#include "tcpcarrier.h"
#include "tcpconnector.h"
#include "tcpprotocol.h"

class Application: public TCPAcceptor::Observer {
public:
	TCPAcceptor *_acceptors;
	std::vector<BaseProtocol*> _inconnections;
	std::vector<BaseProtocol*> _outconnections;
	std::string _type;
	uint32_t _messageLength;
public:
	Application(std::string type, uint32_t messageLength) :
			_acceptors(NULL), _type(type), _messageLength(messageLength) {
	}

	void OnInConnection(BaseProtocol *pProtocol) {
		_inconnections.push_back(pProtocol);
	}
	bool Acceptor(std::string ip, uint16_t port) {

		std::cout << "#-> BindAcceptor() " << ip << " " << port << std::endl;

		ubnt::abstraction::SocketAddress bindAddress(ip, port);
		if (!bindAddress.IsValid()) {
			std::cout << format("Unable to bind on %s:%d\n", ip.c_str(), port) << std::endl;
			return false;
		}

		_acceptors = new TCPAcceptor(bindAddress, SOCKET_TOS_DSCP_EF, this, _messageLength);
		if (!_acceptors->Bind()) {
			std::cout << format("Unable to fire up acceptor to: %s", (const char*) bindAddress) << std::endl;
			return false;
		}
		if (!_acceptors->StartAccept()) {
			printf("Unable to start acceptor\n");
			return false;
		}

		std::cout << "<-# BindAcceptor OK" << std::endl;
		return true;
	}

	static bool SignalProtocolCreated(BaseProtocol *pProtocol) {
		if (pProtocol == NULL) {
			std::cout << format("Connection failed");
			return true;
		}

		pProtocol->EnqueueForHighGranularityTimeEvent(33);

		std::cout << format("Connection success") << std::endl;
		return true;
	}

	bool Connectors(std::string ip, uint16_t port, size_t count) {

		std::cout << "#-> Connectors() " << ip << " " << port << " count=" << count << std::endl;

		for (size_t i = 0; i < count; i++) {
			BaseProtocol *pProtocol = new TCPProtocol("c", _messageLength);
			if (pProtocol == NULL) {
				std::cout << "Unable to create protocol chain" << std::endl;
				return false;
			}
			if (!TCPConnector<Application>::Connect(ip, port, SOCKET_TOS_DSCP_EF, pProtocol)) {
				std::cout << format("Unable to connect to %s:%hu\n", STR(ip), port) << std::endl;
				delete pProtocol;
				return false;
			}
			_outconnections.push_back(pProtocol);
		}
		std::cout << "<-# Connectors OK" << std::endl;
		return true;
	}

	bool Initialize(std::string ip, uint16_t port, size_t count) {
		std::cout << "#-> Initialize() " << std::endl;

		std::cout << "    Initialize I/O handlers manager";
		IOHandlerManager::Initialize();
		std::cout << "    Start I/O handlers manager" << std::endl;
		IOHandlerManager::Start();

		if ((_type == "s") || (_type == "b")) {
			Acceptor(ip, port);
		}
		if ((_type == "c") || (_type == "b")) {
			Connectors(ip, port, count);
		}

		std::cout << "<-# Initialize() " << std::endl;
		return true;
	}

	bool CleanupDeadProtocols() {
		//cout << "#-> CleanupDeadProtocols() " << _inconnections.size() << "/" << _outconnections.size() << " " << std::endl;
		size_t i = 0;
		while (i < _inconnections.size()) {

			if (_inconnections[i]->IsEnqueueForDelete()) {
				delete _inconnections[i];
				_inconnections.erase(_inconnections.begin() + i);
				continue;
			}
			i++;
		}
		i = 0;
		while (i < _outconnections.size()) {

			if (_outconnections[i]->IsEnqueueForDelete()) {
				delete _outconnections[i];
				_outconnections.erase(_outconnections.begin() + i);
				continue;
			}
			i++;
		}

		//cout << "<-# CleanupDeadProtocols() " << _inconnections.size() << "/" << _outconnections.size() << " " << std::endl;
		return true;
	}

	bool Run() {
		while (1) {
			if (!IOHandlerManager::Pulse())
				break;
			IOHandlerManager::DeleteDeadHandlers();
			CleanupDeadProtocols();
		}
		return true;
	}
};

int main(int argc, const char **argv) {
	if (argc < 2) {
		std::cout << "parameters: {type - c,s,b} {count} {ip} {port}" << std::endl;
		return 1;
	}
	std::string type = argv[1];
	size_t count = 1;
	if (argc > 2) {
		count = atoi(argv[2]);
	}
	std::string ip = "127.0.0.1";
	if (argc > 3) {
		ip = argv[3];
	}
	int port = 9999;
	if (argc > 4) {
		port = atoi(argv[4]);
	}
	int messageLength = 20000;
	if (argc > 5) {
		messageLength = atoi(argv[5]);
	}

	Application *app = new Application(type, messageLength);
	app->Initialize(ip, port, count);
	app->Run();
	return 0;
}
