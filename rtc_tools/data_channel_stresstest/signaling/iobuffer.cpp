#include "common.h"
#include "iobuffer.h"

#define WF_IN	0
#define WF_OUT	1

IOBuffer::IOBuffer() {
	_pBuffer = NULL;
	_size = 0;
	_published = 0;
	_consumed = 0;
	_minChunkSize = 4096;
	_sendLimit = 0xffffffff;

	_lastInMs = _lastMs = GetTimeMillis();
}

IOBuffer::~IOBuffer() {
	Cleanup();
}

void IOBuffer::Initialize(uint32_t expected) {
	if ((_pBuffer != NULL) || (_size != 0) || (_published != 0) || (_consumed != 0))
		printf("This buffer was used before. Please initialize it before using\n");
	EnsureSize(expected);
}

bool IOBuffer::ReadFromString(const std::string &binary) {
	if (!ReadFromBuffer((uint8_t *) binary.data(), (uint32_t) binary.length())) {
		return false;
	}
	return true;
}

bool IOBuffer::ReadFromTCPFd(SOCKET_TYPE fd, uint32_t expected, int32_t &recvAmount, int &err) {

	uint64_t start = GetTimeMillis();

	if (expected == 0) {
		err = SOCKET_ERROR_ECONNRESET;

		return false;
	}

	if (_published + expected > _size) {
		if (!EnsureSize(expected)) {

			return false;
		}
	}

	recvAmount = recv(fd, (char*) (_pBuffer + _published), expected, MSG_NOSIGNAL);
	if (recvAmount > 0) {
		_published += (uint32_t) recvAmount;

	} else {
		err = recvAmount == 0 ? SOCKET_ERROR_ECONNRESET : SOCKET_LAST_ERROR;
		if ((err != SOCKET_ERROR_EAGAIN) && (err != SOCKET_ERROR_EINPROGRESS)) {
			return false;
		}
	}
	uint64_t end = GetTimeMillis();

	if ((end - _lastInMs) > 100) {
		std::cout << "    IOBuffer::ReadFromTCPFd read=" << recvAmount << "  prev=" << (end - _lastInMs) << " ms" << std::endl;
	}
	_lastInMs = end;

	if ((end - start) > 10) {
		std::cout << "IOBuffer::ReadFromTCPFd interval=" << (end - start) << " ms " << std::endl;
	}

	return true;
}

bool IOBuffer::ReadFromBuffer(const void *pBuffer, const uint32_t size) {

	if (!EnsureSize(size)) {
		return false;
	}
	memcpy(_pBuffer + _published, pBuffer, size);
	_published += size;

	return true;
}

bool IOBuffer::ReadFromU32(uint32_t value, const bool networkOrder) {
	if (networkOrder)
		value = EHTONL(value);
	return ReadFromBuffer((uint8_t*) &value, sizeof(value));
}

bool IOBuffer::ReadFromRepeat(uint8_t byte, uint32_t size) {

	if (!EnsureSize(size)) {

		return false;
	}
	memset(_pBuffer + _published, byte, size);
	_published += size;

	return true;
}

bool IOBuffer::WriteToTCPFd(SOCKET_TYPE fd, uint32_t size, int32_t &sentAmount, int &err) {

	uint64_t start = GetTimeMillis();

	bool result = true;
	if (_sendLimit != 0xffffffff) {
		size = size > _sendLimit ? _sendLimit : size;
	}
	if (size == 0)
		return true;
	sentAmount = send(fd, (char*) (_pBuffer + _consumed), size > _published - _consumed ? _published - _consumed : size, MSG_NOSIGNAL);

	uint64_t send_end = GetTimeMillis();

	if (sentAmount < 0) {
		err = SOCKET_LAST_ERROR;
		if ((err != SOCKET_ERROR_EAGAIN) && (err != SOCKET_ERROR_EINPROGRESS)) {
			printf("Unable to send %u bytes of data data. Size advertised by network layer was %u. Permanent error (%d): %s", _published - _consumed,
					size, err, strerror(err));
			result = false;
		}
		sentAmount = 0;
	} else {
		_consumed += sentAmount;
		if (_sendLimit != 0xffffffff)
			_sendLimit -= sentAmount;
	}
	if (result)
		Recycle();

	uint64_t end = GetTimeMillis();

	if ((end - _lastMs) > 100) {
		std::cout << "    IOBuffer::WriteToTCPFd sent=" << sentAmount << "  prev=" << (end - _lastMs) << " ms" << std::endl;
	}
	_lastMs = end;

	if ((end - start) > 10) {
		std::cout << "IOBuffer::WriteToTCPFd " << (end - start) << " ms  " << (send_end - start) << " ms" << std::endl;
	}

	return result;
}

bool IOBuffer::Ignore(uint32_t size) {

	_consumed += size;
	if (_sendLimit != 0xffffffff)
		_sendLimit -= size;
	Recycle();

	return true;
}

bool IOBuffer::IgnoreAll() {

	_consumed = _published;
	_sendLimit = 0xffffffff;
	Recycle();

	return true;
}

bool IOBuffer::MoveData() {

	if (_published - _consumed <= _consumed) {
		memcpy(_pBuffer, _pBuffer + _consumed, _published - _consumed);
		_published = _published - _consumed;
		_consumed = 0;
	}

	return true;
}

#define OUTSTANDING (_published - _consumed)
#define AVAILABLE (_size - _published)
#define TOTAL_AVAILABLE (AVAILABLE+_consumed)
#define NEEDED (OUTSTANDING + expected)

bool IOBuffer::EnsureSize(uint32_t expected) {

	if (AVAILABLE >= expected) {
		return true;
	}
	if (TOTAL_AVAILABLE >= expected) {
		MoveData();
		if (AVAILABLE >= expected) {
			return true;
		}
	}
	if (NEEDED < (_size * 1.3)) {
		expected = (uint32_t)(_size * 1.3) - OUTSTANDING;
	}
	if (NEEDED < _minChunkSize) {
		expected = _minChunkSize - OUTSTANDING;
	}
	uint8_t *pTempBuffer = new uint8_t[NEEDED];

	if (_pBuffer != NULL) {
		memcpy(pTempBuffer, _pBuffer + _consumed, OUTSTANDING);
		delete[] _pBuffer;
	}
	_pBuffer = pTempBuffer;

	_size = NEEDED;
	_published = OUTSTANDING;
	_consumed = 0;

	return true;
}

void IOBuffer::Cleanup() {
	if (_pBuffer != NULL) {
		delete[] _pBuffer;
		_pBuffer = NULL;
	}
	_size = 0;
	_published = 0;
	_consumed = 0;
}

void IOBuffer::Recycle() {
	if (_consumed != _published)
		return;
	_consumed = 0;
	_published = 0;

}
