#ifndef NET_DCSCTP_PUBLIC_DCSCTP_MESSAGE_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_MESSAGE_H_

#include <stdint.h>

#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "api/array_view.h"

namespace dcsctp {

// An SCTP message is a group of bytes sent and received as a whole on a
// specified stream identifier (`stream_id`), and with a payload protocol
// identifier (`ppid`).
class DcSctpMessage {
 public:
  DcSctpMessage(uint16_t stream_id, uint32_t ppid, std::vector<uint8_t> payload)
      : stream_id_(stream_id), ppid_(ppid), payload_(std::move(payload)) {}

  DcSctpMessage(DcSctpMessage&& other) = default;
  DcSctpMessage& operator=(DcSctpMessage&& other) = default;
  DcSctpMessage(const DcSctpMessage&) = delete;
  DcSctpMessage& operator=(const DcSctpMessage&) = delete;

  // The stream identifier to which the message is sent.
  uint16_t stream_id() const { return stream_id_; }

  // The payload protocol identifier (ppid) associated with the message.
  uint32_t ppid() const { return ppid_; }

  // The payload of the message.
  rtc::ArrayView<const uint8_t> payload() const { return payload_; }

  // When destructing the message, extracts the payload.
  std::vector<uint8_t> ReleasePayload() && { return std::move(payload_); }

 private:
  uint16_t stream_id_;
  uint32_t ppid_;
  std::vector<uint8_t> payload_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_DCSCTP_MESSAGE_H_
