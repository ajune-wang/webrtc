#include "common.h"
#include "socketaddress.h"

using namespace std;

#define __AS_U8 s6_addr
#define __AS_U16 s6_addr16
#define __AS_U32 s6_addr32

#define IPV6_RAW_8(x) ((uint8_t* )((sockaddr_in6* )& (x))->sin6_addr.__AS_U8)
#define IPV6_RAW_16(x) ((uint16_t* )((sockaddr_in6* )& (x))->sin6_addr.__AS_U16)
#define IPV6_RAW_32(x) ((uint32_t* )((sockaddr_in6* )& (x))->sin6_addr.__AS_U32)

namespace ubnt {
namespace abstraction {

SocketAddress::SocketAddress(int family, const void* pIp, bool isBinaryIp, uint16_t port)
: _port(0),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
_addressLength(0),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
_crc32(0) {
	memset(&_address, 0, sizeof (_address));
	if (pIp == nullptr)
		return;
	switch (family) {
		case AF_INET:
		{
			struct sockaddr_in address;
			memset(&address, 0, sizeof (address));
			address.sin_family = AF_INET;
			if (isBinaryIp) {
				memcpy(&address.sin_addr, pIp, 4);
			} else {
				int err = inet_pton(address.sin_family, (const char*) pIp, &address.sin_addr);
				if (err != 1) {
					Reset();
					return;
				}
			}
			address.sin_port = htons(port);
#ifdef SOCKADDR_HAS_LENGTH_FIELD
			address.sin_len = sizeof (address);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
			Copy((struct sockaddr*) &address);
			break;
		}
		case AF_INET6:
		{
			struct sockaddr_in6 address;
			memset(&address, 0, sizeof (address));
			address.sin6_family = AF_INET6;
			if (isBinaryIp) {
				memcpy(&address.sin6_addr, pIp, 16);
			} else {
				int err = inet_pton(address.sin6_family, (const char*) pIp, &address.sin6_addr);
				if (err != 1) {
					Reset();
					return;
				}
			}
			address.sin6_port = htons(port);
#ifdef SOCKADDR_HAS_LENGTH_FIELD
			address.sin6_len = sizeof (address);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
			Copy((struct sockaddr*) &address);
			break;
		}
		default:
		{
			break;
		}
	}
}

bool SocketAddress::IsIPv6Loopback() const {
	return IsIPv6() && IN6_IS_ADDR_LOOPBACK(&((sockaddr_in6*) & _address)->sin6_addr);
}

bool SocketAddress::IsIPv6IPv4Mapped() const {
	return IsIPv6() && IN6_IS_ADDR_V4MAPPED(&((sockaddr_in6*) & _address)->sin6_addr);
}

bool SocketAddress::IsIPv6UniqueLocal() const {
	return IsIPv6() && ((IPV6_RAW_8(_address)[0] == 0xfc) || (IPV6_RAW_8(_address)[0] == 0xfd));
}

bool SocketAddress::IsIPv6LinkLocal() const {
	return IsIPv6() && IN6_IS_ADDR_LINKLOCAL(&((sockaddr_in6*) & _address)->sin6_addr);
}

bool SocketAddress::IsIPv6Teredo() const {
	return IsIPv6()&&(IPV6_RAW_32(_address)[0] == EHTONL(0x20010000));
}

bool SocketAddress::IsIPv6Benchmarking() const {
	return IsIPv6()&&(IPV6_RAW_32(_address)[0] == EHTONL(0x20010002))
			&&(IPV6_RAW_16(_address)[2] == EHTONS(0x0000))
			;
}

bool SocketAddress::IsIPv6Orchid() const {
	return IsIPv6()&&(IPV6_RAW_32(_address)[0] == EHTONL(0x20010010));
}

bool SocketAddress::IsIPv6To4() const {
	return IsIPv6()&&(IPV6_RAW_16(_address)[0] == EHTONS(0x2002));
}

bool SocketAddress::IsIPv6Documentation() const {
	return IsIPv6()&&(IPV6_RAW_32(_address)[0] == EHTONL(0x20010db8));
}

bool SocketAddress::IsIPv6GlobalUnicast() const {
	return IsIPv6()
			&&((IPV6_RAW_8(_address)[0] >> 5) == 1)
			&&(!IsIPv6Loopback())
			&&(!IsIPv6Loopback())
			&&(!IsIPv6IPv4Mapped())
			&&(!IsIPv6UniqueLocal())
			&&(!IsIPv6LinkLocal())
			&&(!IsIPv6Teredo())
			&&(!IsIPv6Benchmarking())
			&&(!IsIPv6Orchid())
			&&(!IsIPv6To4())
			&&(!IsIPv6Documentation())
			&&(!IsIPv6Multicast())
			;
}

bool SocketAddress::IsIPv6Multicast() const {
	return IsIPv6() && IN6_IS_ADDR_MULTICAST(&((sockaddr_in6*) & _address)->sin6_addr);
}

std::string SocketAddress::SockaddrToString(const struct sockaddr* pAddress,
		std::string* pIp, uint16_t* pPort) {
	if (pAddress == nullptr)
		return "";
	//reset the output variables
	if (pIp != NULL)
		*pIp = "";
	if (pPort != NULL)
		*pPort = 0;

	//see if we need to add the port or not
	bool addPort = ((pAddress->sa_family == AF_INET)&&(((struct sockaddr_in*) pAddress)->sin_port != 0))
			|| ((pAddress->sa_family == AF_INET6)&&(((struct sockaddr_in6*) pAddress)->sin6_port != 0));

	//compute the string ip and port
	char ip[NI_MAXHOST];
	char port[NI_MAXSERV];
#ifdef SOCKADDR_HAS_LENGTH_FIELD
	socklen_t addrLen = pAddress->sa_len;
#else /* SOCKADDR_HAS_LENGTH_FIELD */
	socklen_t addrLen = (pAddress->sa_family == AF_INET6) ? sizeof (struct sockaddr_in6) : sizeof (struct sockaddr_in);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	int err = getnameinfo(pAddress, addrLen, ip, sizeof (ip), port,
			sizeof (port), NI_NUMERICHOST | NI_NUMERICSERV);
	if (err != 0) {
		std::cout << "Error encountered while translating address into string: "
				<< "err: " << err << "; "
				<< "gai_strerror(err): " << gai_strerror(err)
				;
		return "";
	}

	//store the ip and port on the output variables
	if (pIp != NULL)
		*pIp = ip;
	if (pPort != NULL)
		*pPort = ENTOHS(pAddress->sa_family == AF_INET ? (((struct sockaddr_in*) pAddress)->sin_port) : (((struct sockaddr_in6*) pAddress)->sin6_port));

	//compute the complete representation
	std::string result = "";
	if (pAddress->sa_family == AF_INET6) {
		result += "[";
	}
	result += ip;
	if (pAddress->sa_family == AF_INET6) {
		result += "]";
	}
	if (addPort) {
		result += ":";
		result += port;
	}

	//done
	return result;
}

void SocketAddress::Copy(const struct sockaddr* pAddress) {
	_host = "";
	_stringRepresentation = SockaddrToString(pAddress, &_ip, &_port);
#ifdef SOCKADDR_HAS_LENGTH_FIELD
	memcpy(&_address, pAddress, pAddress->sa_len);
	//_crc32 = DigestCRC32Update(0, (uint8_t*) & _address, _address.ss_len);
#else /* SOCKADDR_HAS_LENGTH_FIELD */
	_addressLength = (pAddress->sa_family == AF_INET ? sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6));
	memcpy(&_address, pAddress, _addressLength);
	//_crc32 = DigestCRC32Update(0, (uint8_t*) & _address, _addressLength);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
}

bool SocketAddress::Init(int af, const std::string& address) {
	if (!ParseAndResolve(af, address)) {
		Reset();
		return false;
	}
	_stringRepresentation = SockaddrToString((struct sockaddr*) &_address, NULL, NULL);
#ifdef SOCKADDR_HAS_LENGTH_FIELD
	//_crc32 = DigestCRC32Update(0, (uint8_t*) & _address, _address.ss_len);
#else /* SOCKADDR_HAS_LENGTH_FIELD */
	//_crc32 = DigestCRC32Update(0, (uint8_t*) & _address, _addressLength);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	return true;
}

bool SocketAddress::ParseAndResolve(int af, const std::string& addrStr) {
	//zero everything
	_host = "";
	_ip = "";
	_port = 0;
	memset(&_address, 0, sizeof (_address));
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength = 0;
#endif /* SOCKADDR_HAS_LENGTH_FIELD */

	//sanity check
	if (addrStr.empty())
		return false;

	//determine if this is an IPV6 or not
	bool ipv6 = (addrStr[0] == '[');
	char separator = (ipv6 ? ']' : ':');
	if (ipv6 && (addrStr.size() < 3)) //at least '[1]' for IPV6
		return false;

	//detect the port (if any)
	std::string::size_type pos = addrStr.rfind(separator);
	std::string portString;
	if (pos != std::string::npos) {
		_host = addrStr.substr((ipv6 ? 1 : 0), pos - (ipv6 ? 1 : 0));
		portString = addrStr.substr(pos + 1);
	} else {
		_host = addrStr;
		portString = "0";
	}
	if ((portString.size() >= 2)&&(portString[0] == ':'))
		portString = portString.substr(1);

	if (_host == "")
		return false;

	if (portString == "")
		portString = "0";


	//resolve the address
	struct addrinfo* pTempResult = NULL;
	int err = 0;
	struct addrinfo hints;
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = af;
	if (af == AF_INET6)
		hints.ai_flags |= AI_V4MAPPED;
	if (((err = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &pTempResult)) != 0)
			|| (pTempResult == NULL)) {
		std::cout << "Error encountered while translating string into address: "
				<< "string: `" << _host << ":" << portString << "`; "
				<< "hints.ai_family: " << hints.ai_family << "; "
				<< "err: " << err << "; "
				<< "gai_strerror(err): " << gai_strerror(err)
				;
		return false;
	}

	//store the address and than free resources
	memcpy(&_address, pTempResult->ai_addr, pTempResult->ai_addrlen);
	freeaddrinfo(pTempResult);

	//read the IP as string and the port as number
	socklen_t addrLen = (_address.ss_family == AF_INET6) ? sizeof (struct sockaddr_in6) : sizeof (struct sockaddr_in);
	char ipStr[NI_MAXHOST];
	err = getnameinfo((struct sockaddr*) &_address, addrLen, ipStr, sizeof (ipStr), NULL, 0, NI_NUMERICHOST);
	if (err != 0) {
		std::cout << "Error encountered while translating address into string: "
				<< "err: " << err << "; "
				<< "gai_strerror(err): " << gai_strerror(err)
				;
		return false;
	}
	_ip = ipStr;
	_port = (uint16_t) atoi(portString.c_str());
	if (_address.ss_family == AF_INET6)
		((struct sockaddr_in6*) &_address)->sin6_port = EHTONS(_port);
	else
		((struct sockaddr_in*) &_address)->sin_port = EHTONS(_port);

#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength = (_address.ss_family == AF_INET ? sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6));
#endif /* SOCKADDR_HAS_LENGTH_FIELD */

	//done
	return true;
}

}}
