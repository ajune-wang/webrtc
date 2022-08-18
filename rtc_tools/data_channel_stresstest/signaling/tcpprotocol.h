#pragma once

#include "iotimer.h"
#include "baseprotocol.h"
#include "iohandler.h"
#include "iobuffer.h"

class IOHandler;

class TCPProtocol: public BaseProtocol {
public:
	class Observer {
	public:
		virtual ~Observer() {}
		virtual bool OnMessage(BaseProtocol *pProtocol, uint8_t *buffer, uint32_t size) = 0;
		virtual void OnDisconnect(BaseProtocol *pProtocol) = 0;
	};
private:
	IOHandler *_pCarrier;
	IOBuffer _inputBuffer;
	IOBuffer _outputBuffer;

	IOTimer *_pTimer;
	uint32_t _milliseconds;

	uint64_t _lastOutFrame;
	uint64_t _lastInFrame;

	uint64_t _inFrameMin;
	uint64_t _inFrameMax;

	uint64_t _lastEcho;
	uint64_t _echoMin;
	uint64_t _echoMax;

	uint64_t _lastReport;

	IOBuffer _outputFrame;
	std::string _type;

	uint32_t _messageLength;
	size_t _counter;

	std::map<size_t, uint64_t> _times;

	Observer *_observer;
public:
	TCPProtocol(const std::string &type, Observer *obsever);
	virtual ~TCPProtocol();
	virtual bool Initialize();

	virtual IOHandler* GetIOHandler();

	virtual void SetIOHandler(IOHandler *pIOHandler);

	virtual bool AllowFarProtocol(uint64_t type);
	virtual bool AllowNearProtocol(uint64_t type);

	virtual IOBuffer* GetInputBuffer();
	virtual IOBuffer* GetOutputBuffer();

	virtual bool SignalInputData(int32_t recvAmount);
	virtual bool SignalInputData(IOBuffer &buffer);

	virtual bool SendMessage(std::string msg);
	virtual bool SendOutOfBandData(const IOBuffer &buffer);

	virtual bool EnqueueForOutbound();
	virtual bool TimePeriodElapsed();

	virtual bool EnqueueForTimeEvent(uint32_t seconds);
	virtual bool EnqueueForHighGranularityTimeEvent(uint32_t milliseconds);
	virtual void OnDisconnect();

};
