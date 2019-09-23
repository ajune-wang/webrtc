/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_IP_ADDRESS_H_
#define RTC_BASE_IP_ADDRESS_H_

#if defined(WEBRTC_POSIX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#if defined(WEBRTC_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <string.h>

#include <string>

#include "absl/types/variant.h"
#include "rtc_base/byte_order.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif

namespace rtc {

enum IPv6AddressFlag {
  IPV6_ADDRESS_FLAG_NONE = 0x00,

  // Temporary address is dynamic by nature and will not carry MAC
  // address.
  IPV6_ADDRESS_FLAG_TEMPORARY = 1 << 0,

  // Temporary address could become deprecated once the preferred
  // lifetime is reached. It is still valid but just shouldn't be used
  // to create new connection.
  IPV6_ADDRESS_FLAG_DEPRECATED = 1 << 1,
};

// Version-agnostic IP address class, wraps a union of in_addr and in6_addr.
class IPAddress {
 public:
  IPAddress() = default;
  explicit IPAddress(const in_addr& ip4) : address_(IpV4{ip4}) {}
  explicit IPAddress(uint32_t ip_in_host_byte_order)
      : IPAddress(in_addr{HostToNetwork32(ip_in_host_byte_order)}) {}
  explicit IPAddress(const in6_addr& ip6) : address_(IpV6{ip6}) {}

  IPAddress(const IPAddress& other) = default;
  IPAddress& operator=(const IPAddress& other) = default;

  virtual ~IPAddress() = default;

  friend bool operator==(const IPAddress& lhs, const IPAddress& rhs) {
    return lhs.address_ == rhs.address_;
  }
  friend bool operator!=(const IPAddress& lhs, const IPAddress& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator<(const IPAddress& lhs, const IPAddress& rhs) {
    return lhs.address_ < rhs.address_;
  }
  friend bool operator>(const IPAddress& lhs, const IPAddress& rhs) {
    return rhs.address_ < lhs.address_;
  }

#ifdef UNIT_TEST
  friend std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
      std::ostream& os,             // no-presubmit-check TODO(webrtc:8982)
      const IPAddress& rhs) {
    return os << rhs.ToString();
  }
#endif  // UNIT_TEST

  int family() const {
    struct FamilyVisitor {
      int operator()(const absl::monostate&) { return AF_UNSPEC; }
      int operator()(const IpV4&) { return AF_INET; }
      int operator()(const IpV6&) { return AF_INET6; }
    };
    return absl::visit(FamilyVisitor(), address_);
  }
  in_addr ipv4_address() const { return absl::get<IpV4>(address_).value; }
  in6_addr ipv6_address() const { return absl::get<IpV6>(address_).value; }
  const in6_addr* maybe_ipv6_address() const {
    const IpV6* ip6 = absl::get_if<IpV6>(&address_);
    return ip6 == nullptr ? nullptr : &ip6->value;
  }

  // Returns the number of bytes needed to store the raw address.
  size_t Size() const {
    struct SizeVisitor {
      int operator()(const absl::monostate&) { return 0; }
      int operator()(const IpV4&) { return sizeof(in_addr); }
      int operator()(const IpV6&) { return sizeof(in6_addr); }
    };
    return absl::visit(SizeVisitor(), address_);
  }

  // Wraps inet_ntop.
  std::string ToString() const;

  // Same as ToString but anonymizes it by hiding the last part.
  std::string ToSensitiveString() const;

  // Returns an unmapped address from a possibly-mapped address.
  // Returns the same address if this isn't a mapped address.
  IPAddress Normalized() const;

  // Returns this address as an IPv6 address.
  // Maps v4 addresses (as ::ffff:a.b.c.d), returns v6 addresses unchanged.
  IPAddress AsIPv6Address() const;

  // For socketaddress' benefit. Returns the IP in host byte order.
  uint32_t v4AddressAsHostOrderInteger() const;

  // Whether this is an unspecified IP address.
  bool IsNil() const {
    return absl::holds_alternative<absl::monostate>(address_);
  }

 private:
  // To make the address comparable, wrap system addressed into own structures
  // with defined compare operators.
  struct IpV4 {
    friend bool operator==(const IpV4& lhs, const IpV4& rhs) {
      return std::memcmp(&lhs.value, &rhs.value, sizeof(lhs.value)) == 0;
    }
    friend bool operator<(const IpV4& lhs, const IpV4& rhs) {
      return NetworkToHost32(lhs.value.s_addr) <
             NetworkToHost32(rhs.value.s_addr);
    }
    in_addr value;
  };
  struct IpV6 {
    friend bool operator==(const IpV6& lhs, const IpV6& rhs) {
      return memcmp(&lhs.value, &rhs.value, sizeof(lhs.value)) == 0;
    }
    friend bool operator<(const IpV6& lhs, const IpV6& rhs) {
      return std::memcmp(&lhs.value, &rhs.value, 16) < 0;
    }
    in6_addr value;
  };

  static std::string ToString(const absl::monostate&);
  static std::string ToString(const IpV4& ip4);
  static std::string ToString(const IpV6& ip6);
  static std::string ToSensitiveString(const absl::monostate&);
  static std::string ToSensitiveString(const IpV4& ip4);
  static std::string ToSensitiveString(const IpV6& ip6);

  absl::variant<absl::monostate, IpV4, IpV6> address_;
};

// IP class which could represent IPv6 address flags which is only
// meaningful in IPv6 case.
class InterfaceAddress : public IPAddress {
 public:
  InterfaceAddress() : ipv6_flags_(IPV6_ADDRESS_FLAG_NONE) {}

  explicit InterfaceAddress(IPAddress ip)
      : IPAddress(ip), ipv6_flags_(IPV6_ADDRESS_FLAG_NONE) {}

  InterfaceAddress(IPAddress addr, int ipv6_flags)
      : IPAddress(addr), ipv6_flags_(ipv6_flags) {}

  InterfaceAddress(const in6_addr& ip6, int ipv6_flags)
      : IPAddress(ip6), ipv6_flags_(ipv6_flags) {}

  const InterfaceAddress& operator=(const InterfaceAddress& other);

  bool operator==(const InterfaceAddress& other) const;
  bool operator!=(const InterfaceAddress& other) const;

  int ipv6_flags() const { return ipv6_flags_; }

  std::string ToString() const;

 private:
  int ipv6_flags_;
};

bool IPFromAddrInfo(struct addrinfo* info, IPAddress* out);
bool IPFromString(const std::string& str, IPAddress* out);
bool IPFromString(const std::string& str, int flags, InterfaceAddress* out);
bool IPIsAny(const IPAddress& ip);
bool IPIsLoopback(const IPAddress& ip);
bool IPIsLinkLocal(const IPAddress& ip);
// Identify a private network address like "192.168.111.222"
// (see https://en.wikipedia.org/wiki/Private_network )
bool IPIsPrivateNetwork(const IPAddress& ip);
// Identify a shared network address like "100.72.16.122"
// (see RFC6598)
bool IPIsSharedNetwork(const IPAddress& ip);
// Identify if an IP is "private", that is a loopback
// or an address belonging to a link-local, a private network or a shared
// network.
bool IPIsPrivate(const IPAddress& ip);
bool IPIsUnspec(const IPAddress& ip);
size_t HashIP(const IPAddress& ip);

// These are only really applicable for IPv6 addresses.
bool IPIs6Bone(const IPAddress& ip);
bool IPIs6To4(const IPAddress& ip);
bool IPIsMacBased(const IPAddress& ip);
bool IPIsSiteLocal(const IPAddress& ip);
bool IPIsTeredo(const IPAddress& ip);
bool IPIsULA(const IPAddress& ip);
bool IPIsV4Compatibility(const IPAddress& ip);
bool IPIsV4Mapped(const IPAddress& ip);

// Returns the precedence value for this IP as given in RFC3484.
int IPAddressPrecedence(const IPAddress& ip);

// Returns 'ip' truncated to be 'length' bits long.
IPAddress TruncateIP(const IPAddress& ip, int length);

IPAddress GetLoopbackIP(int family);
IPAddress GetAnyIP(int family);

// Returns the number of contiguously set bits, counting from the MSB in network
// byte order, in this IPAddress. Bits after the first 0 encountered are not
// counted.
int CountIPMaskBits(IPAddress mask);

}  // namespace rtc

#endif  // RTC_BASE_IP_ADDRESS_H_
