#pragma once

#include <string>
#include <functional>
#include <iostream>

#include "iohandler.h"
#include "iohandlermanager.h"
#include "iohandlermanagertoken.h"

#include "tcpacceptor.h"
#include "tcpcarrier.h"
#include "tcpconnector.h"
#include "tcpprotocol.h"

#include "rtc_tools/data_channel_stresstest/signaling/common.h"

class signaling: public TCPAcceptor::Observer, public TCPProtocol::Observer, public TCPConnector::Observer {

	std::string _address;
	uint16_t _port;

	bool _offerer;

	TCPAcceptor *_acceptors;

	std::map<int, BaseProtocol*> _inconnections;
	std::map<int, BaseProtocol*> _outconnections;
private:

public:
	std::function<void(int id)> _on_connect;
	std::function<void(int id, const std::string&)> _on_message;
	std::function<void(int id)> _on_disconnect;

public:
	signaling(std::string address, uint16_t port, bool offerer) :
			_address(address), _port(port), _offerer(offerer), _acceptors(nullptr) {

	}

	virtual ~signaling();

	virtual bool Init();
	virtual bool Run();
	virtual void OnInConnection(BaseProtocol *pProtocol);
	virtual bool OnOutConnection(BaseProtocol *pProtocol);
	virtual bool OnMessage(BaseProtocol *pProtocol, uint8_t *buffer, uint32_t size);
	virtual void OnDisconnect(BaseProtocol *pProtocol);
	virtual bool CleanupDeadProtocols();

	/* callback setters */
	void on_connect(std::function<void(int id)> f) {
		_on_connect = f;
	}

	bool send(int id, const std::string msg);

	void on_message(std::function<void(int id, const std::string&)> f) {
		_on_message = f;
	}
	void on_disconnect(std::function<void(int id)> f) {
		_on_disconnect = f;
	}

	bool start();
};
