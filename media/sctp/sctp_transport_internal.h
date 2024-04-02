/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_SCTP_SCTP_TRANSPORT_INTERNAL_H_
#define MEDIA_SCTP_SCTP_TRANSPORT_INTERNAL_H_

// TODO(deadbeef): Move SCTP code out of media/, and make it not depend on
// anything in media/.

#include <memory>
#include <string>
#include <vector>

#include "api/rtc_error.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/thread.h"

namespace cricket {

// Constants that are important to API users
// The size of the SCTP association send buffer. 256kB, the usrsctp default.
constexpr int kSctpSendBufferSize = 256 * 1024;

// The number of outgoing streams that we'll negotiate. Since stream IDs (SIDs)
// are 0-based, the highest usable SID is 1023.
//
// It's recommended to use the maximum of 65535 in:
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-13#section-6.2
// However, we use 1024 in order to save memory. usrsctp allocates 104 bytes
// for each pair of incoming/outgoing streams (on a 64-bit system), so 65535
// streams would waste ~6MB.
//
// Note: "max" and "min" here are inclusive.
constexpr uint16_t kMaxSctpStreams = 1024;
constexpr uint16_t kMaxSctpSid = kMaxSctpStreams - 1;
constexpr uint16_t kMinSctpSid = 0;
// The maximum number of streams that can be negotiated according to spec.
constexpr uint16_t kSpecMaxSctpSid = 65535;

// This is the default SCTP port to use. It is passed along the wire and the
// connectee and connector must be using the same port. It is not related to the
// ports at the IP level. (Corresponds to: sockaddr_conn.sconn_port in
// usrsctp.h)
const int kSctpDefaultPort = 5000;

// Error cause codes defined at
// https://www.iana.org/assignments/sctp-parameters/sctp-parameters.xhtml#sctp-parameters-24
enum class SctpErrorCauseCode : uint16_t {
  kInvalidStreamIdentifier = 1,
  kMissingMandatoryParameter = 2,
  kStaleCookieError = 3,
  kOutOfResource = 4,
  kUnresolvableAddress = 5,
  kUnrecognizedChunkType = 6,
  kInvalidMandatoryParameter = 7,
  kUnrecognizedParameters = 8,
  kNoUserData = 9,
  kCookieReceivedWhileShuttingDown = 10,
  kRestartWithNewAddresses = 11,
  kUserInitiatedAbort = 12,
  kProtocolViolation = 13,
};

// Abstract SctpTransport interface for use internally (by PeerConnection etc.).
// Exists to allow mock/fake SctpTransports to be created.
class SctpTransportInternal : public webrtc::DataChannelTransportInterface {
 public:
  virtual ~SctpTransportInternal() {}

  virtual void SetOnConnectedCallback(std::function<void()> callback) = 0;
  virtual void SetDataSink(webrtc::DataChannelSink* sink) = 0;

  // When Start is called, connects as soon as possible; this can be called
  // before DTLS completes, in which case the connection will begin when DTLS
  // completes. This method can be called multiple times, though not if either
  // of the ports are changed.
  //
  // `local_sctp_port` and `remote_sctp_port` are passed along the wire and the
  // listener and connector must be using the same port. They are not related
  // to the ports at the IP level. If set to -1, we default to
  // kSctpDefaultPort.
  // `max_message_size_` sets the max message size on the connection.
  // It must be smaller than or equal to kSctpSendBufferSize.
  // It can be changed by a secons Start() call.
  //
  // TODO(deadbeef): Support calling Start with different local/remote ports
  // and create a new association? Not clear if this is something we need to
  // support though. See: https://github.com/w3c/webrtc-pc/issues/979
  virtual bool Start(int local_sctp_port,
                     int remote_sctp_port,
                     int max_message_size) = 0;

  // NOTE: Initially there was a "Stop" method here, but it was never used, so
  // it was removed.

  // Returns the current max message size, set with Start().
  virtual int max_message_size() const = 0;
  // Returns the current negotiated max # of outbound streams.
  // Will return absl::nullopt if negotiation is incomplete.
  virtual absl::optional<int> max_outbound_streams() const = 0;
  // Returns the current negotiated max # of inbound streams.
  virtual absl::optional<int> max_inbound_streams() const = 0;

  // Helper for debugging.
  virtual void set_debug_name_for_testing(const char* debug_name) = 0;
};

}  // namespace cricket

#endif  // MEDIA_SCTP_SCTP_TRANSPORT_INTERNAL_H_
