#include "common.h"
#include "baseprotocol.h"
#include "iohandlertype.h"
#include "tcpcarrier.h"

uint32_t BaseProtocol::_idGenerator = 0;

BaseProtocol::BaseProtocol(uint64_t type) {
	_id = ++_idGenerator;
	_type = type;

	_pFarProtocol = NULL;
	_pNearProtocol = NULL;

	_deleteFar = true;
	_deleteNear = true;

	_enqueueForDelete = false;
	_gracefullyEnqueueForDelete = false;

	_log = false;
	_lastMs = GetTimeMillis();
}

BaseProtocol::~BaseProtocol() {

	BaseProtocol *pFar = _pFarProtocol;
	BaseProtocol *pNear = _pNearProtocol;

	_pFarProtocol = NULL;
	_pNearProtocol = NULL;
	if (pFar != NULL) {
		pFar->_pNearProtocol = NULL;
		if (_deleteFar) {
			pFar->EnqueueForDelete();
		}
	}
	if (pNear != NULL) {
		pNear->_pFarProtocol = NULL;
		if (_deleteNear) {
			pNear->EnqueueForDelete();
		}
	}
}

uint64_t BaseProtocol::GetType() {
	return _type;
}

uint32_t BaseProtocol::GetId() {
	return _id;
}

double BaseProtocol::GetSpawnTimestamp() {
	return _creationTimestamp;
}

void BaseProtocol::SetFarProtocol(BaseProtocol *pProtocol) {
	if (!AllowFarProtocol(pProtocol->_type)) {
		printf("Protocol %s can't accept a far protocol of type: %s", STR(tagToString(_type)), STR(tagToString(pProtocol->_type)));
	}
	if (!pProtocol->AllowNearProtocol(_type)) {
		printf("Protocol %s can't accept a near protocol of type: %s", STR(tagToString(pProtocol->_type)), STR(tagToString(_type)));
	}
	if (_pFarProtocol == NULL) {
		_pFarProtocol = pProtocol;
		pProtocol->SetNearProtocol(this);
	} else {
		if (_pFarProtocol != pProtocol) {
			printf("Far protocol already present");
		}
	}
}
void BaseProtocol::SetNearProtocol(BaseProtocol *pProtocol) {
	if (!AllowNearProtocol(pProtocol->_type)) {
		printf("Protocol %s can't accept a near protocol of type: %s", STR(tagToString(_type)), STR(tagToString(pProtocol->_type)));
	}
	if (!pProtocol->AllowFarProtocol(_type)) {
		printf("Protocol %s can't accept a far protocol of type: %s", STR(tagToString(pProtocol->_type)), STR(tagToString(_type)));
	}
	if (_pNearProtocol == NULL) {
		_pNearProtocol = pProtocol;
		pProtocol->SetFarProtocol(this);
	} else {
		if (_pNearProtocol != pProtocol) {
			printf("Near protocol already present");
		}
	}
}

BaseProtocol* BaseProtocol::GetFarEndpoint() {
	if (_pFarProtocol == NULL) {
		return this;
	} else {
		return _pFarProtocol->GetFarEndpoint();
	}
}

BaseProtocol* BaseProtocol::GetNearEndpoint() {
	if (_pNearProtocol == NULL)
		return this;
	else
		return _pNearProtocol->GetNearEndpoint();
}

void BaseProtocol::EnqueueForDelete() {
	if (_enqueueForDelete)
		return;
	_enqueueForDelete = true;
}

void BaseProtocol::GracefullyEnqueueForDelete(bool fromFarSide) {
	_gracefullyEnqueueForDelete = true;

	if (fromFarSide)
		return GetFarEndpoint()->GracefullyEnqueueForDelete(false);

	if (GetOutputBuffer() != NULL) {
		return;
	}

	if (_pNearProtocol != NULL) {
		_pNearProtocol->GracefullyEnqueueForDelete(false);
	} else {
		EnqueueForDelete();
	}
}

bool BaseProtocol::IsEnqueueForDelete() {
	return _enqueueForDelete || _gracefullyEnqueueForDelete;

}

BaseProtocol::operator std::string() {
	std::string result = "";
	IOHandler *pHandler = NULL;
	if ((pHandler = GetIOHandler()) != NULL) {
		switch (pHandler->GetType()) {
		case IOHT_ACCEPTOR:
			result = format("A(%" PRId64 ") <-> ", (int64_t) pHandler->GetInboundFd());
			break;
		case IOHT_TCP_CARRIER:
			result = format("(Far: %s; Near: %s) CTCP(%" PRId64 ") <-> ",
					(const char *) ((TCPCarrier *) pHandler)->GetFarAddress(),
					(const char *) ((TCPCarrier *) pHandler)->GetNearAddress(),
					(int64_t) pHandler->GetInboundFd());
			break;
		case IOHT_TCP_CONNECTOR:
			result = format("CO(%" PRId64 ") <-> ", (int64_t) pHandler->GetInboundFd());
			break;
		case IOHT_TIMER:
			result = format("T(%" PRId64 ") <-> ", (int64_t) pHandler->GetInboundFd());
			break;
		default:
			result = format("#unknown %hhu#(%" PRId64 ",%" PRId64 ") <-> ",
					pHandler->GetType(),
					(int64_t) pHandler->GetInboundFd(),
					(int64_t) pHandler->GetOutboundFd());
			break;
		}
	}
	BaseProtocol *pTemp = GetFarEndpoint();
	while (pTemp != NULL) {
		pTemp = pTemp->_pNearProtocol;
		if (pTemp != NULL)
			result += " <-> ";
	}
	return result;
}

bool BaseProtocol::Initialize() {
	printf("You should override bool BaseProtocol::Initialize(Variant &parameters) on protocol %s", STR(tagToString(_type)));
	return true;
}

IOHandler* BaseProtocol::GetIOHandler() {
	if (_pFarProtocol != NULL)
		return _pFarProtocol->GetIOHandler();
	return NULL;
}

void BaseProtocol::SetIOHandler(IOHandler *pCarrier) {
	if (_pFarProtocol != NULL)
		_pFarProtocol->SetIOHandler(pCarrier);
}

IOBuffer* BaseProtocol::GetInputBuffer() {
	if (_pFarProtocol != NULL)
		return _pFarProtocol->GetInputBuffer();
	return NULL;
}

IOBuffer* BaseProtocol::GetOutputBuffer() {
	if (_pNearProtocol != NULL)
		return _pNearProtocol->GetOutputBuffer();
	return NULL;
}

bool BaseProtocol::EnqueueForOutbound() {
	if (_pFarProtocol != NULL)
		return _pFarProtocol->EnqueueForOutbound();
	return true;
}

bool BaseProtocol::SendOutOfBandData(const IOBuffer &buffer) {
	printf("Protocol does not support this operation\n");
	return false;
}

bool BaseProtocol::SendMessage(std::string msg) {
	printf("Protocol does not support this operation\n");
	return false;
}

bool BaseProtocol::TimePeriodElapsed() {
	return true;
}

void BaseProtocol::OnDisconnect() {
}
