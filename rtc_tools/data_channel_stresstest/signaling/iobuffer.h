#pragma once

#include "common.h"

#define GETAVAILABLEBYTESCOUNT(x)     ((x)._published - (x)._consumed)
#define GETIBPOINTER(x)     ((uint8_t *)((x)._pBuffer + (x)._consumed))
#define SETIBSENDLIMIT(x,y) \
{ \
	if(((x)._sendLimit!=0)&&((x)._sendLimit!=0xffffffff)){ \
		printf("Setting a IOBufer send limit over an existing limit"); \
	} \
	(x)._sendLimit=(y); \
}
#define GETIBSENDLIMIT(x) ((x)._sendLimit)

class IOBuffer {
public:
	uint8_t *_pBuffer;
	uint32_t _size;
	uint32_t _published;
	uint32_t _consumed;
	uint32_t _minChunkSize;
	uint32_t _sendLimit;

	uint64_t _lastMs;
	uint64_t _lastInMs;

public:
	IOBuffer();
	virtual ~IOBuffer();

	void Initialize(uint32_t expected);

	bool ReadFromTCPFd(SOCKET_TYPE fd, uint32_t expected, int32_t &recvAmount, int &err);
	bool WriteToTCPFd(SOCKET_TYPE fd, uint32_t size, int32_t &sentAmount, int &err);

	bool ReadFromString(const std::string &binary);
	bool ReadFromBuffer(const void *pBuffer, const uint32_t size);
	bool ReadFromU32(uint32_t value, const bool networkOrder);
	bool ReadFromRepeat(uint8_t byte, uint32_t size);

	bool Ignore(uint32_t size);
	bool IgnoreAll();
	bool MoveData();
	bool EnsureSize(uint32_t expected);

protected:
	void Cleanup();
	void Recycle();
};
