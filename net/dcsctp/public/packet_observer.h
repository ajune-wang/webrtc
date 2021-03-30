#ifndef NET_DCSCTP_PUBLIC_PACKET_OBSERVER_H_
#define NET_DCSCTP_PUBLIC_PACKET_OBSERVER_H_

#include <stdint.h>

#include "api/array_view.h"

namespace dcsctp {

// A PacketObserver can be attached to a socket and will be called for
// all sent and received packets.
class PacketObserver {
 public:
  virtual ~PacketObserver() = default;
  // Called when a packet is sent, with the current time (in milliseconds) as
  // `time_ms`, and the packet payload as `payload`.
  virtual void OnSentPacket(int64_t time_ms,
                            rtc::ArrayView<const uint8_t> payload) = 0;

  // Called when a packet is received, with the current time (in milliseconds)
  // as `time_ms`, and the packet payload as `payload`.
  virtual void OnReceivedPacket(int64_t time_ms,
                                rtc::ArrayView<const uint8_t> payload) = 0;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_PACKET_OBSERVER_H_
