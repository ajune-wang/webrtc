/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_ANDROID)
#include "rtc_base/ifaddrs_android.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>  // no-presubmit-check
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>  // no-presubmit-check
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "absl/cleanup/cleanup.h"
#include "rtc_base/logging.h"

namespace {

struct netlinkrequest {
  nlmsghdr header;
  ifaddrmsg msg;
};

const int kMaxReadSize = 4096;

// Helper function to extract IPv4 address and prefix length from CIDR strings
// CIDR format example: "192.1.1.34/24", where 24 is prefix length
bool ParseCidrString(const std::string& cidr_string,
                     in_addr* ip_address,
                     int* prefix_length) {
  size_t slash_pos = cidr_string.find('/');
  if (slash_pos == std::string::npos) {
    return false;
  }

  std::string ip_string = cidr_string.substr(0, slash_pos);
  std::string prefix_string = cidr_string.substr(slash_pos + 1);

  if (inet_aton(ip_string.c_str(), ip_address) == 0) {
    return false;
  }

  *prefix_length = std::stoi(prefix_string);
  return true;
}

// Function to check for and return the ARC++ system property on ChromeOS for
// the IPv4 addresses, which are not otherwise visible to the network stack.
// Example properties values:
//   [vendor.arc.net.ipv4.host_eth4_address]: [100.127.127.229/24]
//   [vendor.arc.net.ipv4.host_eth5_address]: [100.87.84.74/24]
// Returns true if the property was found and parsed, false otherwise.
bool GetOverrideIpAddress(char* interface_name,
                          in_addr* ip_address,
                          int* prefix_length) {
  rtc::StringBuilder ss;
  ss << "vendor.arc.net.ipv4.host_" << interface_name << "_address";
  std::string property_name = ss.Release();

  const prop_info* pi = __system_property_find(property_name.c_str());
  if (pi != nullptr) {
    char* property_value = new char[PROP_VALUE_MAX];
    __system_property_get(property_name.c_str(), property_value);
    RTC_LOG(LS_INFO) << "Overridden IPv4 address for " << interface_name << ": "
                     << property_value << ".";
    bool result = ParseCidrString(property_value, ip_address, prefix_length);
    delete[] property_value;
    return result;
  }
  return false;
}

}  // namespace

namespace rtc {

int set_ifname(struct ifaddrs* ifaddr, int interface) {
  char buf[IFNAMSIZ] = {0};
  char* name = if_indextoname(interface, buf);
  if (name == nullptr) {
    return -1;
  }
  ifaddr->ifa_name = new char[strlen(name) + 1];
  strncpy(ifaddr->ifa_name, name, strlen(name) + 1);
  return 0;
}

int set_flags(struct ifaddrs* ifaddr) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    return -1;
  }
  ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifaddr->ifa_name, IFNAMSIZ - 1);
  int rc = ioctl(fd, SIOCGIFFLAGS, &ifr);
  close(fd);
  if (rc == -1) {
    return -1;
  }
  ifaddr->ifa_flags = ifr.ifr_flags;
  return 0;
}

int set_addresses(struct ifaddrs* ifaddr,
                  ifaddrmsg* msg,
                  void* data,
                  size_t len) {
  // On ChromeOS devices with ARC++, check if the IPv4 address should
  // be overriden with the value contained in the Android system property
  if (msg->ifa_family == AF_INET) {
    in_addr override_ip;
    int override_prefix_length;
    if (GetOverrideIpAddress(ifaddr->ifa_name, &override_ip,
                             &override_prefix_length)) {
      sockaddr_in* sa = new sockaddr_in;
      sa->sin_family = AF_INET;
      sa->sin_addr = override_ip;  // Override IP address
      ifaddr->ifa_addr = reinterpret_cast<sockaddr*>(sa);
      msg->ifa_prefixlen = override_prefix_length;  // Override prefix length
      return 0;
    }
  }

  // Default behavior if no override property is found
  if (msg->ifa_family == AF_INET) {
    sockaddr_in* sa = new sockaddr_in;
    sa->sin_family = AF_INET;
    memcpy(&sa->sin_addr, data, len);
    ifaddr->ifa_addr = reinterpret_cast<sockaddr*>(sa);
  } else if (msg->ifa_family == AF_INET6) {
    sockaddr_in6* sa = new sockaddr_in6;
    sa->sin6_family = AF_INET6;
    sa->sin6_scope_id = msg->ifa_index;
    memcpy(&sa->sin6_addr, data, len);
    ifaddr->ifa_addr = reinterpret_cast<sockaddr*>(sa);
  } else {
    return -1;
  }
  return 0;
}

int make_prefixes(struct ifaddrs* ifaddr, int family, int prefixlen) {
  char* prefix = nullptr;
  if (family == AF_INET) {
    sockaddr_in* mask = new sockaddr_in;
    mask->sin_family = AF_INET;
    memset(&mask->sin_addr, 0, sizeof(in_addr));
    ifaddr->ifa_netmask = reinterpret_cast<sockaddr*>(mask);
    if (prefixlen > 32) {
      prefixlen = 32;
    }
    prefix = reinterpret_cast<char*>(&mask->sin_addr);
  } else if (family == AF_INET6) {
    sockaddr_in6* mask = new sockaddr_in6;
    mask->sin6_family = AF_INET6;
    memset(&mask->sin6_addr, 0, sizeof(in6_addr));
    ifaddr->ifa_netmask = reinterpret_cast<sockaddr*>(mask);
    if (prefixlen > 128) {
      prefixlen = 128;
    }
    prefix = reinterpret_cast<char*>(&mask->sin6_addr);
  } else {
    return -1;
  }
  for (int i = 0; i < (prefixlen / 8); i++) {
    *prefix++ = 0xFF;
  }
  char remainder = 0xff;
  remainder <<= (8 - prefixlen % 8);
  *prefix = remainder;
  return 0;
}

int populate_ifaddrs(struct ifaddrs* ifaddr,
                     ifaddrmsg* msg,
                     void* bytes,
                     size_t len) {
  if (set_ifname(ifaddr, msg->ifa_index) != 0) {
    return -1;
  }
  if (set_flags(ifaddr) != 0) {
    return -1;
  }
  if (set_addresses(ifaddr, msg, bytes, len) != 0) {
    return -1;
  }
  if (make_prefixes(ifaddr, msg->ifa_family, msg->ifa_prefixlen) != 0) {
    return -1;
  }
  return 0;
}

int getifaddrs(struct ifaddrs** result) {
  *result = nullptr;
  int fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    return -1;
  }
  absl::Cleanup close_file = [fd] { close(fd); };

  netlinkrequest ifaddr_request;
  memset(&ifaddr_request, 0, sizeof(ifaddr_request));
  ifaddr_request.header.nlmsg_flags = NLM_F_ROOT | NLM_F_REQUEST;
  ifaddr_request.header.nlmsg_type = RTM_GETADDR;
  ifaddr_request.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));

  ssize_t count = send(fd, &ifaddr_request, ifaddr_request.header.nlmsg_len, 0);
  if (static_cast<size_t>(count) != ifaddr_request.header.nlmsg_len) {
    return -1;
  }
  struct ifaddrs* start = nullptr;
  absl::Cleanup cleanup_start = [&start] { freeifaddrs(start); };
  struct ifaddrs* current = nullptr;
  char buf[kMaxReadSize];
  ssize_t amount_read = recv(fd, &buf, kMaxReadSize, 0);
  while (amount_read > 0) {
    nlmsghdr* header = reinterpret_cast<nlmsghdr*>(&buf[0]);
    size_t header_size = static_cast<size_t>(amount_read);
    for (; NLMSG_OK(header, header_size);
         header = NLMSG_NEXT(header, header_size)) {
      switch (header->nlmsg_type) {
        case NLMSG_DONE:
          // Success. Return `start`. Cancel `start` cleanup  because it
          // becomes callers responsibility.
          std::move(cleanup_start).Cancel();
          *result = start;
          return 0;
        case NLMSG_ERROR:
          return -1;
        case RTM_NEWADDR: {
          ifaddrmsg* address_msg =
              reinterpret_cast<ifaddrmsg*>(NLMSG_DATA(header));
          rtattr* rta = IFA_RTA(address_msg);
          ssize_t payload_len = IFA_PAYLOAD(header);
          while (RTA_OK(rta, payload_len)) {
            if ((address_msg->ifa_family == AF_INET &&
                 rta->rta_type == IFA_LOCAL) ||
                (address_msg->ifa_family == AF_INET6 &&
                 rta->rta_type == IFA_ADDRESS)) {
              ifaddrs* newest = new ifaddrs;
              memset(newest, 0, sizeof(ifaddrs));
              if (current) {
                current->ifa_next = newest;
              } else {
                start = newest;
              }
              if (populate_ifaddrs(newest, address_msg, RTA_DATA(rta),
                                   RTA_PAYLOAD(rta)) != 0) {
                return -1;
              }
              current = newest;
            }
            rta = RTA_NEXT(rta, payload_len);
          }
          break;
        }
      }
    }
    amount_read = recv(fd, &buf, kMaxReadSize, 0);
  }
  return -1;
}

void freeifaddrs(struct ifaddrs* addrs) {
  struct ifaddrs* last = nullptr;
  struct ifaddrs* cursor = addrs;
  while (cursor) {
    delete[] cursor->ifa_name;
    delete cursor->ifa_addr;
    delete cursor->ifa_netmask;
    last = cursor;
    cursor = cursor->ifa_next;
    delete last;
  }
}

}  // namespace rtc
#endif  // defined(WEBRTC_ANDROID)
