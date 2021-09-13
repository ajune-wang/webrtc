/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_DCSCTP_HANDOVER_STATE_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_HANDOVER_STATE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace dcsctp {

// Stores state snapshot of a dcsctp socket. The snapshot can be used to
// recreate the socket possibly in another process. This state should be
// treaded as opaque - dcsctp's user code should not inspect or alter it except
// for serialization. Serialization is not provided by dcsctp. If needed it has
// to be implemented in dcsctp's user code.
struct DcSctpSocketHandoverState {
  struct OrderedStream {
    uint32_t id = 0;
    uint32_t next_ssn = 0;
  };
  struct UnorderedStream {
    uint32_t id = 0;
  };
  struct Receive {
    std::vector<OrderedStream> ordered_streams;
    std::vector<UnorderedStream> unordered_streams;
  };
  Receive rx;
};

// Return value of DcSctpSocketInterface::GetHandoverReadiness. Bitset. When no
// bit is set, the socket is in the state in which a snapshot of the state can
// be made by `GetHandoverStateAndClose()`.
enum class HandoverReadinessStatus : uint32_t {
  kReady = 0,
  kWrongConnectionState = 1,
  kSendQueueNotEmpty = 2,
  kDataTrackerNotIdle = 4,
  kDataTrackerTsnBlocksPending = 8,
  kReassemblyQueueNotEmpty = 16,
  kReassemblyQueueDeliveredTSNsGap = 32,
  kStreamResetDeferred = 64,
  kOrderedStreamHasUnassembledChunks = 128,
  kUnorderedStreamHasUnassembledChunks = 256,
  kRetransmissionQueueOutstandingData = 512,
  kRetransmissionQueueFastRecovery = 1024,
  kRetransmissionQueueNotEmpty = 2048,
  kMax = kRetransmissionQueueNotEmpty,
};

inline constexpr HandoverReadinessStatus Combine(HandoverReadinessStatus s1,
                                                 HandoverReadinessStatus s2) {
  return static_cast<HandoverReadinessStatus>(static_cast<uint32_t>(s1) |
                                              static_cast<uint32_t>(s2));
}

}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_DCSCTP_HANDOVER_STATE_H_
