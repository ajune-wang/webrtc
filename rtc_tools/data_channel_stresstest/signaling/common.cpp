#include "common.h"
uint64_t GetTimeMicros() {
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (uint64_t) ts.tv_sec * 1000000 + (uint64_t) ts.tv_usec;
}

bool setFdKeepAlive(SOCKET_TYPE fd, bool isUdp) {
	if (isUdp)
		return true;
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char *) &one, sizeof(one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with SOL_SOCKET/SO_KEEPALIVE failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with SOL_SOCKET/SO_KEEPALIVE failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
	return true;
}

bool setFdNoNagle(SOCKET_TYPE fd, bool isUdp) {
	if (isUdp)
		return true;
	int one = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof(one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with IPPROTO_TCP/TCP_NODELAY failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with IPPROTO_TCP/TCP_NODELAY failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
	return true;
}

bool setFdReuseAddress(SOCKET_TYPE fd) {
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with SOL_SOCKET/SO_REUSEADDR failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with SOL_SOCKET/SO_REUSEADDR failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const char *) &one, sizeof (one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with SOL_SOCKET/SO_REUSEPORT failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with SOL_SOCKET/SO_REUSEPORT failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
#endif /* SO_REUSEPORT */
	return true;
}

bool setFdTTL(SOCKET_TYPE fd, int af, uint8_t ttl) {
	int temp = ttl;
	if ((af != AF_INET) && (af != AF_INET6)) {
		printf("Invalid socket domain. Allowed values are AF_INET and AF_INET6");
		return false;
	}
	int level = af == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
	int option = af == AF_INET ? IP_TTL : IPV6_UNICAST_HOPS;
	if (setsockopt(fd, level, option, (const char *) &temp, sizeof(temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with IPPROTO_IP/IP_TTL failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with IPPROTO_IP/IP_TTL failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setFdMulticastTTL(SOCKET_TYPE fd, uint8_t ttl) {
	int temp = ttl;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &temp, sizeof(temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with IPPROTO_IP/IP_MULTICAST_TTL failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with IPPROTO_IP/IP_MULTICAST_TTL failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setIPv4TOS(SOCKET_TYPE fd, uint8_t tos) {
	int temp = tos;
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, (const char *) &temp, sizeof(temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with IPPROTO_IP/IP_TOS failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with IPPROTO_IP/IP_TOS failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setIPv6TOS(SOCKET_TYPE fd, uint8_t tos) {
#ifdef WIN32
	return true;
#else /* WIN32 */
	int temp = tos;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, (const char *) &temp, sizeof(temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
		printf("setsockopt() with IPPROTO_IPV6/IPV6_TCLASS failed. Error was: (%d) %s", err, strerror(err));
	}
	return true;
#endif /* WIN32 */
}

bool setIPTOS(SOCKET_TYPE fd, uint8_t tos, bool isIpV6) {
#if 0 //British Telecom does not like TOS
	return isIpV6
	? setIPv6TOS(fd, tos)
	: setIPv4TOS(fd, tos)
	;
#else
	return true;
#endif
}

bool setFdLinger(SOCKET_TYPE fd) {
	struct linger temp;
	temp.l_onoff = 0;
	temp.l_linger = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char*) &temp, sizeof(temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() with SOL_SOCKET/SO_LINGER failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() with SOL_SOCKET/SO_LINGER failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

#define MIN_SOCK_BUF_SIZE (256 * 1024)
#define MAX_SOCK_BUF_SIZE (2 * 1024 * 1024)

bool setFdBuff(SOCKET_TYPE fd, int option, int size) {
	size = std::max(MIN_SOCK_BUF_SIZE, size);
	size = std::min(MAX_SOCK_BUF_SIZE, size);
	if (setsockopt(fd, SOL_SOCKET, option, (const char*) &size, sizeof(size)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		printf("setsockopt() failed. Error was: %d", err);
#else /* WIN32 */
		printf("setsockopt() failed. Error was: (%d) %s\n", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
	return true;
}

bool setFdSndRcvBuff(SOCKET_TYPE fd, uint32_t sendSize, uint32_t recvSize) {
	return ((sendSize > 0) ? setFdBuff(fd, SO_SNDBUF, sendSize) : true) && ((recvSize > 0) ? setFdBuff(fd, SO_RCVBUF, recvSize) : true);
}

bool setFdNoSIGPIPE(SOCKET_TYPE fd) {
	//int32_t one = 1;
	//if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char*) &one, sizeof(one)) != 0) {
	//	printf("Unable to set SO_NOSIGPIPE\n");
	//	return false;
	//}
	return true;
}

bool setFdMinSendBuff(SOCKET_TYPE fd, int size, bool isUdp) {
	return setFdBuff(fd, SO_SNDBUF, size);
}

bool setFdMaxSndRcvBuff(SOCKET_TYPE fd, bool isUdp) {
	return setFdBuff(fd, SO_SNDBUF, MAX_SOCK_BUF_SIZE) && setFdBuff(fd, SO_RCVBUF, MAX_SOCK_BUF_SIZE);
}

bool setFdOptions(SOCKET_TYPE fd, bool isUdp) {
	setFdNoNagle(fd, isUdp);
	if (!isUdp)
		setFdLinger(fd);

	if ((!setFdNonBlock(fd)) && (!isUdp))
		return false;

	if ((!setFdNoSIGPIPE(fd)) || (!setFdKeepAlive(fd, isUdp)) || (!setFdReuseAddress(fd)) || (!setFdMaxSndRcvBuff(fd, isUdp)))
		return false;
	return true;
}

bool setFdCloseOnExec(SOCKET_TYPE fd) {
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		int err = errno;
		printf("fcntl failed %d %s\n", err, strerror(err));
		return false;
	}
	return true;
}

bool setFdNonBlock(SOCKET_TYPE fd) {
	int32_t arg;
	if ((arg = fcntl(fd, F_GETFL, NULL)) < 0) {
		int err = errno;
		printf("Unable to get fd flags: (%d) %s\n", err, strerror(err));
		return false;
	}
	arg |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, arg) < 0) {
		int err = errno;
		printf("Unable to set fd flags: (%d) %s\n", err, strerror(err));
		return false;
	}

	return true;
}

uint64_t getTagMask(uint64_t tag) {
	uint64_t result = 0xffffffffffffffffULL;
	for (int8_t i = 56; i >= 0; i -= 8) {
		if (((tag >> i) & 0xff) == 0)
			break;
		result = result >> 8;
	}
	return ~result;
}

std::string tagToString(uint64_t tag) {
	std::string result;
	for (uint32_t i = 0; i < 8; i++) {
		uint8_t v = (tag >> ((7 - i) * 8) & 0xff);
		if (v == 0)
			break;
		result += (char) v;
	}
	return result;
}

std::string format(const char *pFormat, ...) {
	char *pBuffer = NULL;
	va_list arguments;
	va_start(arguments, pFormat);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif /* __clang__ */
	if (vasprintf(&pBuffer, pFormat, arguments) == -1) {
		va_end(arguments);
		return "";
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
	va_end(arguments);
	std::string result;
	if (pBuffer != NULL) {
		result = pBuffer;
		free(pBuffer);
	}
	return result;
}
