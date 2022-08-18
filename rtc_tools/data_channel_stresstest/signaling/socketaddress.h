#pragma once

#include "common.h"
//#define SOCKADDR_HAS_LENGTH_FIELD 1

namespace ubnt {
namespace abstraction {


class SocketAddress {

	template<typename T, typename... Ts>
	struct is_one_of {
		static constexpr bool value = false;
	};

	template<typename T, typename U, typename... Rs>
	struct is_one_of<T, U, Rs...> {
		static constexpr bool value = std::is_same<T, U>::value
				|| is_one_of<T, Rs...>::value;
	};
private:
	std::string _host;
	std::string _ip;
	uint16_t _port;
	std::string _stringRepresentation;
	struct sockaddr_storage _address;

#ifndef SOCKADDR_HAS_LENGTH_FIELD
	/**
	 * The length of the _address field for the systems which are not having a
	 * corresponding field in the _address itself
	 */
	size_t _addressLength;
#endif /* SOCKADDR_HAS_LENGTH_FIELD */

	/**
	 * 32bit CRC of the address which is computed on the raw bytes of the _address
	 * field
	 */
	uint32_t _crc32;
public:

	/**
	 * Constructs a SocketAddress object and leaves it empty
	 */
	inline SocketAddress()
	: _port(0),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength(0),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	_crc32(0) {
		memset(&_address, 0, sizeof (_address));
	}

	/**
	 * Standard copy constructor
	 * @param address the address object to be copied
	 */
	inline SocketAddress(const SocketAddress& val)
	: _host(val._host),
	_ip(val._ip),
	_port(val._port),
	_stringRepresentation(val._stringRepresentation),
	_address(val._address),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength(val._addressLength),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	_crc32(val._crc32) {
	}

	/**
	 * Standard move constructor
	 * @param val
	 */
	inline SocketAddress(SocketAddress&& val) noexcept
	: _host(std::move(val._host)),
	_ip(std::move(val._ip)),
	_port(val._port),
	_stringRepresentation(std::move(val._stringRepresentation)),
	_address(val._address),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength(val._addressLength),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	_crc32(val._crc32) {
	}

	/**
	 * Constructs a SocketAddress object and initializes it with the provided value.
	 * @param pAddress the address used to initialize the object
	 */
	template<typename AddressType>
	inline SocketAddress(const AddressType* pAddress)
	: _port(0),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength(0),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	_crc32(0) {
		memset(&_address, 0, sizeof (_address));
		if (pAddress == nullptr)
			return;
		Copy(pAddress);
	}

	/**
	 * Constructs a SocketAddress object and initializes it with the provided value.
	 * @param af the address family of the wanted resolved address. PF_UNSPEC can
	 * be used to accept any kind of address family. PF_INET and PF_INET6 are
	 * also supported
	 * @param hostOrIp the string representation in the form of {IPv4|IPv6}[:PORT]
	 */
	inline SocketAddress(int af, const std::string& address)
	: _port(0),
#ifndef SOCKADDR_HAS_LENGTH_FIELD
	_addressLength(0),
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	_crc32(0) {
		memset(&_address, 0, sizeof (_address));
		Init(af, address);
	}

	/**
	 * Constructs a SocketAddress object and initializes it with the provided ip,
	 * port and family
	 * @param family the family
	 * @param pIp the ip in binary network order representation. The length of this
	 * is deducted from the family parameter
	 * @param isBinaryIp if true, than pIp will be treated as binary representation
	 * of the address (4 or 16 bytes) in network order. If false, than pIp is
	 * treated as the literal/text representation of the address.
	 * @param port the port of the address
	 */
	SocketAddress(int family, const void* pIp, bool isBinaryIp, uint16_t port);

	/**
	 * Constructs a SocketAddress object and initializes it with the provided ip,
	 * and port
	 * @param ip the string representation of the IP. If it is IPv6, it must be
	 * unboxed. Example:
	 *		good: "::1";
	 *		bad: "[::1]"
	 * @param port the port of the address
	 */
	inline SocketAddress(const std::string& ip, uint16_t port)
	: SocketAddress(ip.find(':') == std::string::npos ? AF_INET : AF_INET6, ip.c_str(), false, port) {
	}

	/**
	 * Standard destructor
	 */
	virtual ~SocketAddress() = default;

	/**
	 * Standard copy assignment operator
	 * @param val the value to be copied into this instance
	 * @return current object
	 */
	SocketAddress& operator=(const SocketAddress& val) {
		if (this == &val)
			return *this;
		_host = val._host;
		_ip = val._ip;
		_port = val._port;
		_stringRepresentation = val._stringRepresentation;
		_address = val._address;
#ifndef SOCKADDR_HAS_LENGTH_FIELD
		_addressLength = val._addressLength;
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
		_crc32 = val._crc32;
		return *this;
	}

	/**
	 * Standard move assignment operator
	 * @param val the value to be moved inside this instance
	 * @return current object
	 */
	inline SocketAddress& operator=(SocketAddress&& val) noexcept {
		if (this == &val)
			return *this;
		_host .swap(val._host);
		_ip .swap(val._ip);
		_port = val._port;
		_stringRepresentation.swap(val._stringRepresentation);
		_address = val._address;
#ifndef SOCKADDR_HAS_LENGTH_FIELD
		_addressLength = val._addressLength;
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
		_crc32 = val._crc32;
		return *this;
	}

	/**
	 * Assignment operator from a struct sockaddr value
	 * @param address the struct sockaddr_xxx value to be assigned
	 * @return current object
	 */
	template<typename AddressType>
	inline SocketAddress& operator=(const AddressType* pAddress) {
		if (pAddress == nullptr) {
			Reset();
			return *this;
		}
		Copy(pAddress);
		return *this;
	}

	/*
	 * Comparison operators section
	 */
	inline bool operator==(const char* pAddress) const {
		return pAddress == nullptr
				? (!IsValid())
				: (*this == std::string(pAddress))
				;
	}

	inline bool operator==(const std::string& address) const {
		return address.empty()
				? (!IsValid())
				: (*this == SocketAddress(GetFamily(), address))
				;
	}

	inline bool operator==(const SocketAddress& address) const {
		if ((this == &address)
				|| ((!IsValid())&&(!address.IsValid()))
				)
			return true;
		return IsValid()
				&& address.IsValid()
				&& (GetLength() == address.GetLength())
				&& (_crc32 == address._crc32)
				&&(memcmp(&_address, &address._address, GetLength()) == 0)
				;
	}

	template<typename AddressType>
	inline typename std::enable_if<is_one_of<AddressType,
	struct sockaddr,
	struct sockaddr_in,
	struct sockaddr_in6,
	struct sockaddr_storage
	>::value, bool>::type
	operator==(const AddressType* pAddress) const {
		const sockaddr_storage* pAddressStorage = (const sockaddr_storage*) pAddress;
		if (pAddressStorage == nullptr)
			return !IsValid();
		return (_address.ss_family == pAddressStorage->ss_family)
#ifdef SOCKADDR_HAS_LENGTH_FIELD
				&&(_address.ss_len == pAddressStorage->ss_len)
				&&(memcmp(&_address, pAddressStorage, _address.ss_len) == 0)
#else /* SOCKADDR_HAS_LENGTH_FIELD */
				&&(_addressLength == (_address.ss_family == AF_INET ? sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6)))
				&&(memcmp(&_address, pAddressStorage, _addressLength) == 0)
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
				;
	}

	template<typename AddressType>
	inline typename std::enable_if<is_one_of<AddressType,
	struct sockaddr,
	struct sockaddr_in,
	struct sockaddr_in6,
	struct sockaddr_storage
	>::value, bool>::type
	operator!=(const AddressType* pAddress) const {
		return !(*this == pAddress);
	}

	inline bool operator!=(const SocketAddress& address) const {
		return !(*this == address);
	}

	/*
	 * Cast operators section
	 */
	inline operator const char* () const {
		return _stringRepresentation.c_str();
	}

	inline operator const std::string& () const {
		return _stringRepresentation;
	}

	inline operator const struct sockaddr* () const {
		return ((_address.ss_family == AF_INET) || (_address.ss_family == AF_INET6)) ? (struct sockaddr*) &_address : NULL;
	}

	inline operator const struct sockaddr_in* () const {
		return _address.ss_family == AF_INET ? ((struct sockaddr_in*) &_address) : NULL;
	}

	inline operator const struct sockaddr_in6* () const {
		return _address.ss_family == AF_INET6 ? ((struct sockaddr_in6*) &_address) : NULL;
	}

	/**
	 * Converts the current address to an IPv6 address. If the current address
	 * is already IPv6 this operation will be the equivalent of a logical copy
	 * operation. If the current address is IPv4 than IPv6 V4MAPPED address will
	 * be computed and saved. If the current address is IPv4 and there is no IPv6
	 * support, than destination will be reset and left uninitialized.
	 * @param dest the destination for the computed address
	 */
	inline void ConvertToIPV6(SocketAddress& dest) {
		dest = SocketAddress(PF_INET6, _stringRepresentation);
		dest._host = _host;
		if (!dest.IsIPv6())
			dest.Reset();
	}

	/**
	 * Gets the family of the address
	 * @return the family of the address
	 */
	inline int GetFamily() const {
		return ((_address.ss_family == AF_INET) || (_address.ss_family == AF_INET6)) ? _address.ss_family : 0;
	}

	/**
	 * Gets the host of the address
	 * @return the host of the address
	 */
	inline const std::string& GetHost() const {
		return _host;
	}

	/**
	 * Gets the IP of the address
	 * @return the IP of the address
	 */
	inline const std::string& GetIp() const {
		return _ip;
	}

	/**
	 * Gets the port of the address
	 * @return the port of the address
	 */
	inline uint16_t GetPort() const {
		return _port;
	}

	/**
	 * Gets the length of the backend struct sockaddr_storage
	 * @return the length of the backend struct sockaddr_storage
	 */
	inline socklen_t GetLength() const {
#ifdef SOCKADDR_HAS_LENGTH_FIELD
		return (socklen_t) (((_address.ss_family == AF_INET) || (_address.ss_family == AF_INET6)) ? _address.ss_len : 0);
#else /* SOCKADDR_HAS_LENGTH_FIELD */
		return (socklen_t) (((_address.ss_family == AF_INET) || (_address.ss_family == AF_INET6)) ? _addressLength : 0);
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
	}

	/**
	 * Gets the CRC32 value which is computed on top of the backend struct sockaddr
	 * storage
	 * @return the CRC32 value which is computed on top of the backend struct sockaddr
	 * storage
	 */
	inline uint32_t GetCRC32() const {
		return _crc32;
	}

	/**
	 * Checks if the address is initialized
	 * @return true if initialized, false otherwise
	 */
	inline bool IsValid() const {
		return IsIPv4() || IsIPv6();
	}

	/**
	 * Checks if the address is ipv4
	 * @return true if the address is ipv4, false otherwise
	 */
	inline bool IsIPv4() const {
		return _address.ss_family == AF_INET;
	}

	/**
	 * Checks if the address is a ipv4 multicast
	 * @return true if the address is a ipv4 multicast, false otherwise
	 */
	inline bool IsIPv4Multicast() const {
		if (!IsIPv4())
			return false;
		uint32_t testVal = ENTOHL(((sockaddr_in*) & _address)->sin_addr.s_addr);
		return (testVal > 0xe0000000) && (testVal < 0xefffffff);
	}

	/**
	 * Checks if the address is ipv4 loopback
	 * @return true if the address is ipv4 loopback, false otherwise
	 */
	inline bool IsIPv4Loopback() const {
		if (!IsIPv4())
			return false;
		uint32_t testVal = ENTOHL(((sockaddr_in*) & _address)->sin_addr.s_addr);
		return (testVal >> 24) == 0x7f;
	}

	/**
	 * Checks if the address is ipv6
	 * @return true if the address is ipv6, false otherwise
	 */
	inline bool IsIPv6() const {
		return _address.ss_family == AF_INET6;
	}

	/**
	 * Checks if the address is ANY address which is 0.0.0.0 for IPv4
	 * and :: for IPv6
	 * @return true if the address is ANY address, false otherwise
	 */
	inline bool IsAny() const {
		if (!IsValid())
			return false;
		static const uint8_t zeroBlock[16] = {0};
		switch (_address.ss_family) {
			case AF_INET:
				return memcmp((const void*) (&(((struct sockaddr_in*) &_address)->sin_addr)), zeroBlock, 4) == 0;
			case AF_INET6:
				return memcmp((const void*) (&(((struct sockaddr_in6*) &_address)->sin6_addr)), zeroBlock, 16) == 0;
			default:
				return false;
		}
	}

	/**
	 * Checks if the address is ipv4 or ipv6 loopback
	 * @return true if the address is ipv4 or ipv6 loopback, false otherwise
	 */
	bool IsLoopback() const {
		switch (_address.ss_family) {
			case AF_INET:
				return IsIPv4Loopback();
			case AF_INET6:
				return IsIPv6Loopback();
			default:
				return false;
		}
	}

	/**
	 * Checks if the address is ipv6 loopback
	 * @return true if the address is ipv6 loopback, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Loopback() const;

	/**
	 * Checks if the address is a ipv4 mapped onto a ipv6
	 * @return true if the the address is a ipv4 mapped onto a ipv6, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6IPv4Mapped() const;

	/**
	 * Checks if the address is a ipv6 unique local
	 * @return true if the address is a ipv6 unique local, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6UniqueLocal() const;

	/**
	 * Checks if the address is a ipv6 link loal
	 * @return true if the address is a ipv6 link local, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6LinkLocal() const;

	/**
	 * Checks if the address is a ipv6 teredo tunnel
	 * @return true if the address is a ipv6 teredo tunnel, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Teredo() const;

	/**
	 * Checks if the address is a ipv6 documentation/benchmarking
	 * @return true if the address is a ipv6 documentation/benchmarking, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Benchmarking() const;

	/**
	 * Checks if the address is a ipv6 orchid (fixed term experiment)
	 * @return true if the address is a ipv6 orchid  (fixed term experiment), false
	 * otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Orchid() const;

	/**
	 * Checks if the address is a ipv6 mapped from an ipv4
	 * @return true if the address is a ipv6 mapped from an ipv4, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6To4() const;

	/**
	 * Checks if the address is a ipv6 documentation/benchmarking
	 * @return true if the address is a ipv6 documentation/benchmarking, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Documentation() const;

	/**
	 * Checks if the address is a ipv6 global unicast
	 * @return true if the address is a ipv6 global unicast, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6GlobalUnicast() const;

	/**
	 * Checks if the address is a ipv6 multicast
	 * @return true if the address is a ipv6 multicast, false otherwise
	 * @referenceDoc: <root>/docs/ipv6_reference_card.pdf
	 */
	bool IsIPv6Multicast() const;

	/**
	 * Un-initializes the address
	 */
	inline void Reset() {
		_host = "";
		_ip = "";
		_port = 0;
		_stringRepresentation = "";
		memset(&_address, 0, sizeof (_address));
#ifndef SOCKADDR_HAS_LENGTH_FIELD
		_addressLength = 0;
#endif /* SOCKADDR_HAS_LENGTH_FIELD */
		_crc32 = 0;
	}

	/**
	 * Converts a sockaddr structure to the canonical string representation. The
	 * IPv6 values will be in the form of [<ip>]:<port> while the ipv4 will be in
	 * the form of <ip>:<port>. If the port is 0 inside the provideod sockaddr object,
	 * than the :<port> part from the end result will be omitted.
	 * @param pAddress the address to be converted to string representation
	 * @param pIp output variable which will be set to the string representation
	 * of the ip. It will be ignored if NULL.
	 * @param pPort output variable which will be set to the numerical representation
	 * of the port. It will be ignored if NULL.
	 * @return the canonical string representation of the ip/port pair
	 */
	static std::string SockaddrToString(const struct sockaddr* pAddress, std::string* pIp,
			uint16_t* pPort);
private:

	template<typename AddressType>
	inline typename std::enable_if<is_one_of<AddressType,
	struct sockaddr_in,
	struct sockaddr_in6,
	struct sockaddr_storage
	>::value, void>::type
	Copy(const AddressType* pAddress) {
		Copy((const struct sockaddr*) pAddress);
	}
	void Copy(const struct sockaddr* pAddress);
	bool Init(int af, const std::string& address);
	bool ParseAndResolve(int af, const std::string& addrStr);
};

}
}
