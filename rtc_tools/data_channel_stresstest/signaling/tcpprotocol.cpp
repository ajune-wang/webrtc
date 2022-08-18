#include "common.h"
#include "tcpprotocol.h"
#include "iohandler.h"
#include "iohandlertype.h"

const uint32_t HEADER_SIZE = 8;
//const uint32_t REPORT_INTERVAL_MS = 3000;

TCPProtocol::TCPProtocol(const std::string &type, Observer *observer) :
		BaseProtocol(PT_TCP), _type(type), _observer(observer) {
	_pCarrier = NULL;
	_pTimer = new IOTimer();
	_pTimer->SetProtocol(this);
	_milliseconds = 0;
	_lastReport = _lastInFrame = _lastOutFrame = _lastEcho = GetTimeMillis();

	_inFrameMin = 10000000000000;
	_inFrameMax = 0;
	_echoMin = 10000000000000;
	_echoMax = 0;

	_counter = 0;
	_messageLength = 0;
}

TCPProtocol::~TCPProtocol() {
	if (_pCarrier != NULL) {
		IOHandler *pCarrier = _pCarrier;
		_pCarrier = NULL;
		pCarrier->SetProtocol(NULL);
		delete pCarrier;
	}
}

bool TCPProtocol::Initialize() {
	return true;
}

IOHandler* TCPProtocol::GetIOHandler() {
	return _pCarrier;
}

void TCPProtocol::SetIOHandler(IOHandler *pIOHandler) {
	if (pIOHandler != NULL) {
		if ((pIOHandler->GetType() != IOHT_TCP_CARRIER) && (pIOHandler->GetType() != IOHT_STDIO)) {
			printf("This protocol accepts only TCP carriers\n");
		}
	}
	_pCarrier = pIOHandler;
}

bool TCPProtocol::AllowFarProtocol(uint64_t type) {
	printf("This protocol doesn't accept any far protocol\n");
	return false;
}

bool TCPProtocol::AllowNearProtocol(uint64_t type) {
	return true;
}

IOBuffer* TCPProtocol::GetInputBuffer() {
	return &_inputBuffer;
}

IOBuffer* TCPProtocol::GetOutputBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_outputBuffer) != 0)
		return &_outputBuffer;
	return NULL;
}

bool TCPProtocol::SignalInputData(int32_t recvAmount) {

	std::cout << "#-> TCPProtocol::SignalInputData id=" << GetId() << " recv=" << recvAmount << " avail=" << GETAVAILABLEBYTESCOUNT(_inputBuffer)
			<< std::endl;

	int i = 0;
	while (true) {
		i++;

		std::cout << "    TCPProtocol::SignalInputData id=" << GetId() << " " << i << ") " << std::endl;

		uint32_t size = GETAVAILABLEBYTESCOUNT(_inputBuffer);

		if (size < HEADER_SIZE) {
			std::cout << "    TCPProtocol::SignalInputData id=" << GetId() << " size=" << size << " HEADER_SIZE=" << HEADER_SIZE << std::endl;
			break;
		}

		if ((ENTOHLP(GETIBPOINTER(_inputBuffer)) != 0x464C5600)) {
			std::cout << "<-# TCPProtocol::SignalInputData id=" << GetId() << " Header invalid" << std::endl;
			return false;
		}

		uint32_t length = ENTOHLP(GETIBPOINTER(_inputBuffer) + 4);

		if (size < length + HEADER_SIZE) {
			std::cout << "    TCPProtocol::SignalInputData id=" << GetId() << " size=" << size << " < length=" << length << " + HEADER_SIZE="
					<< HEADER_SIZE << std::endl;
			break;
		}

		if (_observer) {
			_observer->OnMessage(this, GETIBPOINTER(_inputBuffer) + HEADER_SIZE, length);
		} else {
			std::cout << "    TCPProtocol::SignalInputData id=" << GetId() << " " << i << ") NO OBSERVER" << std::endl;
		}

		_inputBuffer.Ignore(length + HEADER_SIZE);
	}

	std::cout << "<-# TCPProtocol::SignalInputData id=" << GetId() << " recv=" << recvAmount << " avail=" << GETAVAILABLEBYTESCOUNT(_inputBuffer)
			<< std::endl;

	return true;
}

bool TCPProtocol::SendOutOfBandData(const IOBuffer &buffer) {
	if (!_outputBuffer.ReadFromBuffer(GETIBPOINTER(buffer), GETAVAILABLEBYTESCOUNT(buffer))) {
		return false;
	}
	if (!EnqueueForOutbound()) {
		return false;
	}
	return true;
}

bool TCPProtocol::SignalInputData(IOBuffer&/* ignored */) {
	printf("OPERATION NOT SUPPORTED");
	return false;
}

bool TCPProtocol::EnqueueForOutbound() {
	if (_pCarrier == NULL) {
		printf("TCPProtocol has no carrier");
		return false;
	}
	return _pCarrier->SignalOutputData();
}

bool TCPProtocol::SendMessage(std::string msg) {

	uint32_t length = msg.length();

	std::cout << "TCPProtocol::SendMessage() len=" << length << " | " << msg << std::endl;

	_outputFrame.IgnoreAll();
	_outputFrame.ReadFromU32(0x464C5600, true);
	_outputFrame.ReadFromU32(length, true);
	_outputFrame.ReadFromString(msg);

	if (!SendOutOfBandData(_outputFrame)) {
		return false;
	}

	return true;
}

bool TCPProtocol::TimePeriodElapsed() {
	return true;
}

bool TCPProtocol::EnqueueForTimeEvent(uint32_t seconds) {
	if (_pTimer == NULL) {
		printf("TCPProtocol has no timer\n");
		return false;
	}
	_milliseconds = seconds * 1000;
	return _pTimer->EnqueueForTimeEvent(seconds);
}

bool TCPProtocol::EnqueueForHighGranularityTimeEvent(uint32_t milliseconds) {
	if (_pTimer == NULL) {
		printf("BaseTimerProtocol has no timer");
		return false;
	}
	_milliseconds = milliseconds;
	return _pTimer->EnqueueForHighGranularityTimeEvent(milliseconds);
}

void TCPProtocol::OnDisconnect() {
	if (_observer) {
		_observer->OnDisconnect(this);
	}
}
