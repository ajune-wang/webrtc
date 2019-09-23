/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_POSIX)
#include <netinet/in.h>
#include <sys/socket.h>
#ifdef OPENBSD
#include <netinet/in_systm.h>
#endif
#ifndef __native_client__
#include <netinet/ip.h>
#endif
#include <netdb.h>
#endif

#include "rtc_base/byte_order.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/string_utils.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

namespace rtc {
namespace {
// Prefixes used for categorizing IPv6 addresses.
constexpr in6_addr kV4MappedPrefix = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0}}};
constexpr in6_addr k6To4Prefix = {{{0x20, 0x02, 0}}};
constexpr in6_addr kTeredoPrefix = {{{0x20, 0x01, 0x00, 0x00}}};
constexpr in6_addr kV4CompatibilityPrefix = {{{0}}};
constexpr in6_addr k6BonePrefix = {{{0x3f, 0xfe, 0}}};
constexpr in6_addr kPrivateNetworkPrefix = {{{0xFD}}};

bool IPIsHelper(const in6_addr& addr, const in6_addr& tomatch, int length) {
  // Helper method for checking IP prefix matches (but only on whole byte
  // lengths). Length is in bits.
  return ::memcmp(&addr, &tomatch, (length >> 3)) == 0;
}

bool IPIsHelper(const IPAddress& ip, const in6_addr& tomatch, int length) {
  // Helper method for checking IP prefix matches (but only on whole byte
  // lengths). Length is in bits.
  const in6_addr* addr = ip.maybe_ipv6_address();
  return addr && IPIsHelper(*addr, tomatch, length);
}

bool IP6IsV4Mapped(const in6_addr& ip) {
  return IPIsHelper(ip, kV4MappedPrefix, 96);
}

in_addr ExtractMappedAddress(const in6_addr& in6) {
  in_addr ipv4;
  ::memcpy(&ipv4.s_addr, &in6.s6_addr[12], sizeof(ipv4.s_addr));
  return ipv4;
}
}  // namespace

uint32_t IPAddress::v4AddressAsHostOrderInteger() const {
  struct V4AddressVisitor {
    uint32_t operator()(const absl::monostate&) { return 0; }
    uint32_t operator()(const IpV4& ip4) {
      return NetworkToHost32(ip4.value.s_addr);
    }
    uint32_t operator()(const IpV6&) { return 0; }
  };
  return absl::visit(V4AddressVisitor(), address_);
}

bool IPIs6Bone(const IPAddress& ip) {
  return IPIsHelper(ip, k6BonePrefix, 16);
}

bool IPIs6To4(const IPAddress& ip) {
  return IPIsHelper(ip, k6To4Prefix, 16);
}

std::string IPAddress::ToString(const absl::monostate&) {
  return std::string();
}

std::string IPAddress::ToString(const IpV4& ip4) {
  char buf[INET6_ADDRSTRLEN] = {0};
  if (!rtc::inet_ntop(AF_INET, &ip4.value, buf, sizeof(buf))) {
    return std::string();
  }
  return std::string(buf);
}

std::string IPAddress::ToString(const IpV6& ip6) {
  char buf[INET6_ADDRSTRLEN] = {0};
  if (!rtc::inet_ntop(AF_INET6, &ip6.value, buf, sizeof(buf))) {
    return std::string();
  }
  return std::string(buf);
}

std::string IPAddress::ToSensitiveString(const absl::monostate&) {
  return std::string();
}

std::string IPAddress::ToSensitiveString(const IpV4& ip4) {
  std::string address = ToString(ip4);
  size_t find_pos = address.rfind('.');
  if (find_pos == std::string::npos)
    return std::string();
  address.resize(find_pos);
  address += ".x";
  return address;
}

std::string IPAddress::ToSensitiveString(const IpV6& ip6) {
  std::string result;
  result.resize(INET6_ADDRSTRLEN);
  const auto& addr = ip6.value.s6_addr;
  size_t len = snprintf(&(result[0]), result.size(), "%x:%x:%x:x:x:x:x:x",
                        (addr[0] << 8) + addr[1], (addr[2] << 8) + addr[3],
                        (addr[4] << 8) + addr[5]);
  result.resize(len);
  return result;
}

std::string IPAddress::ToString() const {
  return absl::visit([](const auto& ip) { return ToString(ip); }, address_);
}

std::string IPAddress::ToSensitiveString() const {
#if !defined(NDEBUG)
  // Return non-stripped in debug.
  return ToString();
#else
  return absl::visit([](const auto& ip) { return ToSensitiveString(ip); },
                     address_);
#endif
}

IPAddress IPAddress::Normalized() const {
  const IpV6* ip6 = absl::get_if<IpV6>(&address_);
  if (ip6 != nullptr && IP6IsV4Mapped(ip6->value)) {
    return IPAddress(ExtractMappedAddress(ip6->value));
  }
  return *this;
}

IPAddress IPAddress::AsIPv6Address() const {
  const IpV4* ip4 = absl::get_if<IpV4>(&address_);
  if (ip4 == nullptr) {
    return *this;
  }
  in6_addr v6addr = kV4MappedPrefix;
  ::memcpy(&v6addr.s6_addr[12], &ip4->value.s_addr, sizeof(ip4->value.s_addr));
  return IPAddress(v6addr);
}

bool InterfaceAddress::operator==(const InterfaceAddress& other) const {
  return ipv6_flags_ == other.ipv6_flags() &&
         static_cast<const IPAddress&>(*this) == other;
}

bool InterfaceAddress::operator!=(const InterfaceAddress& other) const {
  return !((*this) == other);
}

const InterfaceAddress& InterfaceAddress::operator=(
    const InterfaceAddress& other) {
  ipv6_flags_ = other.ipv6_flags_;
  static_cast<IPAddress&>(*this) = other;
  return *this;
}

std::string InterfaceAddress::ToString() const {
  std::string result = IPAddress::ToString();

  if (family() == AF_INET6)
    result += "|flags:0x" + rtc::ToHex(ipv6_flags());

  return result;
}

static bool IPIsPrivateNetworkV4(const IPAddress& ip) {
  uint32_t ip_in_host_order = ip.v4AddressAsHostOrderInteger();
  return ((ip_in_host_order >> 24) == 10) ||
         ((ip_in_host_order >> 20) == ((172 << 4) | 1)) ||
         ((ip_in_host_order >> 16) == ((192 << 8) | 168));
}

static bool IPIsPrivateNetworkV6(const IPAddress& ip) {
  return IPIsHelper(ip, kPrivateNetworkPrefix, 8);
}

bool IPIsPrivateNetwork(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return IPIsPrivateNetworkV4(ip);
    }
    case AF_INET6: {
      return IPIsPrivateNetworkV6(ip);
    }
  }
  return false;
}

static bool IPIsSharedNetworkV4(const IPAddress& ip) {
  uint32_t ip_in_host_order = ip.v4AddressAsHostOrderInteger();
  return (ip_in_host_order >> 22) == ((100 << 2) | 1);
}

bool IPIsSharedNetwork(const IPAddress& ip) {
  if (ip.family() == AF_INET) {
    return IPIsSharedNetworkV4(ip);
  }
  return false;
}

bool IPFromAddrInfo(struct addrinfo* info, IPAddress* out) {
  if (!info || !info->ai_addr) {
    return false;
  }
  if (info->ai_addr->sa_family == AF_INET) {
    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
    *out = IPAddress(addr->sin_addr);
    return true;
  } else if (info->ai_addr->sa_family == AF_INET6) {
    sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(info->ai_addr);
    *out = IPAddress(addr->sin6_addr);
    return true;
  }
  return false;
}

bool IPFromString(const std::string& str, IPAddress* out) {
  if (!out) {
    return false;
  }
  in_addr addr;
  if (rtc::inet_pton(AF_INET, str.c_str(), &addr) == 0) {
    in6_addr addr6;
    if (rtc::inet_pton(AF_INET6, str.c_str(), &addr6) == 0) {
      *out = IPAddress();
      return false;
    }
    *out = IPAddress(addr6);
  } else {
    *out = IPAddress(addr);
  }
  return true;
}

bool IPFromString(const std::string& str, int flags, InterfaceAddress* out) {
  IPAddress ip;
  if (!IPFromString(str, &ip)) {
    return false;
  }

  *out = InterfaceAddress(ip, flags);
  return true;
}

bool IPIsAny(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET:
      return ip == IPAddress(INADDR_ANY);
    case AF_INET6:
      return ip == IPAddress(in6addr_any) || ip == IPAddress(kV4MappedPrefix);
    case AF_UNSPEC:
      return false;
  }
  return false;
}

static bool IPIsLoopbackV4(const IPAddress& ip) {
  uint32_t ip_in_host_order = ip.v4AddressAsHostOrderInteger();
  return ((ip_in_host_order >> 24) == 127);
}

static bool IPIsLoopbackV6(const IPAddress& ip) {
  return ip == IPAddress(in6addr_loopback);
}

bool IPIsLoopback(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return IPIsLoopbackV4(ip);
    }
    case AF_INET6: {
      return IPIsLoopbackV6(ip);
    }
  }
  return false;
}

bool IPIsPrivate(const IPAddress& ip) {
  return IPIsLinkLocal(ip) || IPIsLoopback(ip) || IPIsPrivateNetwork(ip) ||
         IPIsSharedNetwork(ip);
}

bool IPIsUnspec(const IPAddress& ip) {
  return ip.family() == AF_UNSPEC;
}

size_t HashIP(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return ip.ipv4_address().s_addr;
    }
    case AF_INET6: {
      in6_addr v6addr = ip.ipv6_address();
      const uint32_t* v6_as_ints =
          reinterpret_cast<const uint32_t*>(&v6addr.s6_addr);
      return v6_as_ints[0] ^ v6_as_ints[1] ^ v6_as_ints[2] ^ v6_as_ints[3];
    }
  }
  return 0;
}

IPAddress TruncateIP(const IPAddress& ip, int length) {
  if (length < 0) {
    return IPAddress();
  }
  if (ip.family() == AF_INET) {
    if (length > 31) {
      return ip;
    }
    if (length == 0) {
      return IPAddress(INADDR_ANY);
    }
    int mask = (0xFFFFFFFF << (32 - length));
    uint32_t host_order_ip = NetworkToHost32(ip.ipv4_address().s_addr);
    in_addr masked;
    masked.s_addr = HostToNetwork32(host_order_ip & mask);
    return IPAddress(masked);
  } else if (ip.family() == AF_INET6) {
    if (length > 127) {
      return ip;
    }
    if (length == 0) {
      return IPAddress(in6addr_any);
    }
    in6_addr v6addr = ip.ipv6_address();
    int position = length / 32;
    int inner_length = 32 - (length - (position * 32));
    // Note: 64bit mask constant needed to allow possible 32-bit left shift.
    uint32_t inner_mask = 0xFFFFFFFFLL << inner_length;
    uint32_t* v6_as_ints = reinterpret_cast<uint32_t*>(&v6addr.s6_addr);
    for (int i = 0; i < 4; ++i) {
      if (i == position) {
        uint32_t host_order_inner = NetworkToHost32(v6_as_ints[i]);
        v6_as_ints[i] = HostToNetwork32(host_order_inner & inner_mask);
      } else if (i > position) {
        v6_as_ints[i] = 0;
      }
    }
    return IPAddress(v6addr);
  }
  return IPAddress();
}

int CountIPMaskBits(IPAddress mask) {
  uint32_t word_to_count = 0;
  int bits = 0;
  switch (mask.family()) {
    case AF_INET: {
      word_to_count = NetworkToHost32(mask.ipv4_address().s_addr);
      break;
    }
    case AF_INET6: {
      in6_addr v6addr = mask.ipv6_address();
      const uint32_t* v6_as_ints =
          reinterpret_cast<const uint32_t*>(&v6addr.s6_addr);
      int i = 0;
      for (; i < 4; ++i) {
        if (v6_as_ints[i] != 0xFFFFFFFF) {
          break;
        }
      }
      if (i < 4) {
        word_to_count = NetworkToHost32(v6_as_ints[i]);
      }
      bits = (i * 32);
      break;
    }
    default: {
      return 0;
    }
  }
  if (word_to_count == 0) {
    return bits;
  }

  // Public domain bit-twiddling hack from:
  // http://graphics.stanford.edu/~seander/bithacks.html
  // Counts the trailing 0s in the word.
  unsigned int zeroes = 32;
  // This could also be written word_to_count &= -word_to_count, but
  // MSVC emits warning C4146 when negating an unsigned number.
  word_to_count &= ~word_to_count + 1;  // Isolate lowest set bit.
  if (word_to_count)
    zeroes--;
  if (word_to_count & 0x0000FFFF)
    zeroes -= 16;
  if (word_to_count & 0x00FF00FF)
    zeroes -= 8;
  if (word_to_count & 0x0F0F0F0F)
    zeroes -= 4;
  if (word_to_count & 0x33333333)
    zeroes -= 2;
  if (word_to_count & 0x55555555)
    zeroes -= 1;

  return bits + (32 - zeroes);
}

static bool IPIsLinkLocalV4(const IPAddress& ip) {
  uint32_t ip_in_host_order = ip.v4AddressAsHostOrderInteger();
  return ((ip_in_host_order >> 16) == ((169 << 8) | 254));
}

static bool IPIsLinkLocalV6(const IPAddress& ip) {
  // Can't use the helper because the prefix is 10 bits.
  in6_addr addr = ip.ipv6_address();
  return (addr.s6_addr[0] == 0xFE) && ((addr.s6_addr[1] & 0xC0) == 0x80);
}

bool IPIsLinkLocal(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return IPIsLinkLocalV4(ip);
    }
    case AF_INET6: {
      return IPIsLinkLocalV6(ip);
    }
  }
  return false;
}

// According to http://www.ietf.org/rfc/rfc2373.txt, Appendix A, page 19.  An
// address which contains MAC will have its 11th and 12th bytes as FF:FE as well
// as the U/L bit as 1.
bool IPIsMacBased(const IPAddress& ip) {
  in6_addr addr = ip.ipv6_address();
  return ((addr.s6_addr[8] & 0x02) && addr.s6_addr[11] == 0xFF &&
          addr.s6_addr[12] == 0xFE);
}

bool IPIsSiteLocal(const IPAddress& ip) {
  // Can't use the helper because the prefix is 10 bits.
  const in6_addr* addr = ip.maybe_ipv6_address();
  return addr && addr->s6_addr[0] == 0xFE && (addr->s6_addr[1] & 0xC0) == 0xC0;
}

bool IPIsULA(const IPAddress& ip) {
  // Can't use the helper because the prefix is 7 bits.
  const in6_addr* addr = ip.maybe_ipv6_address();
  return addr && (addr->s6_addr[0] & 0xFE) == 0xFC;
}

bool IPIsTeredo(const IPAddress& ip) {
  return IPIsHelper(ip, kTeredoPrefix, 32);
}

bool IPIsV4Compatibility(const IPAddress& ip) {
  return IPIsHelper(ip, kV4CompatibilityPrefix, 96);
}

bool IPIsV4Mapped(const IPAddress& ip) {
  return IPIsHelper(ip, kV4MappedPrefix, 96);
}

int IPAddressPrecedence(const IPAddress& ip) {
  // Precedence values from RFC 3484-bis. Prefers native v4 over 6to4/Teredo.
  if (ip.family() == AF_INET) {
    return 30;
  } else if (ip.family() == AF_INET6) {
    if (IPIsLoopback(ip)) {
      return 60;
    } else if (IPIsULA(ip)) {
      return 50;
    } else if (IPIsV4Mapped(ip)) {
      return 30;
    } else if (IPIs6To4(ip)) {
      return 20;
    } else if (IPIsTeredo(ip)) {
      return 10;
    } else if (IPIsV4Compatibility(ip) || IPIsSiteLocal(ip) || IPIs6Bone(ip)) {
      return 1;
    } else {
      // A 'normal' IPv6 address.
      return 40;
    }
  }
  return 0;
}

IPAddress GetLoopbackIP(int family) {
  if (family == AF_INET) {
    return rtc::IPAddress(INADDR_LOOPBACK);
  }
  if (family == AF_INET6) {
    return rtc::IPAddress(in6addr_loopback);
  }
  return rtc::IPAddress();
}

IPAddress GetAnyIP(int family) {
  if (family == AF_INET) {
    return rtc::IPAddress(INADDR_ANY);
  }
  if (family == AF_INET6) {
    return rtc::IPAddress(in6addr_any);
  }
  return rtc::IPAddress();
}

}  // namespace rtc
