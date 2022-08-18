#pragma once

#include "common.h"
#include "iohandler.h"

class IOBuffer;
class IOHandler;

class BaseProtocol {
private:
	static uint32_t _idGenerator;
	uint32_t _id;

protected:
	uint64_t _type;
	BaseProtocol *_pFarProtocol;
	BaseProtocol *_pNearProtocol;
	bool _deleteFar;
	bool _deleteNear;
	bool _enqueueForDelete;
	bool _gracefullyEnqueueForDelete;
	double _creationTimestamp;

public:
	bool _log;
	std::string _name;
	uint64_t _lastMs;

	BaseProtocol(uint64_t type);
	virtual ~BaseProtocol();

	uint64_t GetType();
	uint32_t GetId();
	double GetSpawnTimestamp();

	void SetFarProtocol(BaseProtocol *pProtocol);
	void SetNearProtocol(BaseProtocol *pProtocol);

	BaseProtocol *GetFarEndpoint();
	BaseProtocol *GetNearEndpoint();

	bool IsEnqueueForDelete();
	operator std::string();

	virtual bool Initialize();
	virtual void EnqueueForDelete();
	virtual void GracefullyEnqueueForDelete(bool fromFarSide = true);

	virtual IOHandler *GetIOHandler();
	virtual void SetIOHandler(IOHandler *pCarrier);

	virtual IOBuffer * GetInputBuffer();
	virtual IOBuffer * GetOutputBuffer();

	virtual bool EnqueueForOutbound();

	virtual bool SendOutOfBandData(const IOBuffer &buffer);
	virtual bool SendMessage(std::string msg);

	virtual bool AllowFarProtocol(uint64_t type) = 0;
	virtual bool AllowNearProtocol(uint64_t type) = 0;
	virtual bool SignalInputData(int32_t recvAmount) = 0;
	virtual bool SignalInputData(IOBuffer &buffer) = 0;

	virtual bool EnqueueForTimeEvent(uint32_t seconds) = 0;
	virtual bool EnqueueForHighGranularityTimeEvent(uint32_t milliseconds) = 0;
	virtual bool TimePeriodElapsed();
	virtual void OnDisconnect();
};
