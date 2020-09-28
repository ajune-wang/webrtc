/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TRANSCEIVER_LIST_H_
#define PC_TRANSCEIVER_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "pc/rtp_transceiver.h"

namespace webrtc {

typedef rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
    TransPtr;

// Captures partial state to be used for rollback. Applicable only in
// Unified Plan.
class TransceiverStableState {
 public:
  TransceiverStableState() {}
  void set_newly_created();
  void SetMSectionIfUnset(absl::optional<std::string> mid,
                          absl::optional<size_t> mline_index);
  void SetRemoteStreamIdsIfUnset(const std::vector<std::string>& ids);
  absl::optional<std::string> mid() const { return mid_; }
  absl::optional<size_t> mline_index() const { return mline_index_; }
  absl::optional<std::vector<std::string>> remote_stream_ids() const {
    return remote_stream_ids_;
  }
  bool has_m_section() const { return has_m_section_; }
  bool newly_created() const { return newly_created_; }

 private:
  absl::optional<std::string> mid_;
  absl::optional<size_t> mline_index_;
  absl::optional<std::vector<std::string>> remote_stream_ids_;
  // Indicates that mid value from stable state has been captured and
  // that rollback has to restore the transceiver. Also protects against
  // subsequent overwrites.
  bool has_m_section_ = false;
  // Indicates that the transceiver was created as part of applying a
  // description to track potential need for removing transceiver during
  // rollback.
  bool newly_created_ = false;
};

class TransceiverList {
 public:
  std::vector<TransPtr> List() const { return transceivers_; }

  void Add(TransPtr transceiver) { transceivers_.push_back(transceiver); }
  void Remove(TransPtr transceiver) {
    int remaining_transceiver_count = 0;
    for (auto&& t : transceivers_) {
      if (t != transceiver) {
        transceivers_[remaining_transceiver_count++] = t;
      }
    }
    transceivers_.resize(remaining_transceiver_count);
  }

  TransceiverStableState* StableState(TransPtr transceiver) {
    return &(transceiver_stable_states_by_transceivers_[transceiver]);
  }

  void DiscardStableStates() {
    transceiver_stable_states_by_transceivers_.clear();
  }

  std::map<TransPtr, TransceiverStableState>& StableStates() {
    return transceiver_stable_states_by_transceivers_;
  }

 private:
  std::vector<TransPtr> transceivers_;
  // Holds changes made to transceivers during applying descriptors for
  // potential rollback. Gets cleared once signaling state goes to stable.
  std::map<TransPtr, TransceiverStableState>
      transceiver_stable_states_by_transceivers_;
  // Holds remote stream ids for transceivers from stable state.
  std::map<TransPtr, std::vector<std::string>>
      remote_stream_ids_by_transceivers_;
};

}  // namespace webrtc

#endif  // PC_TRANSCEIVER_LIST_H_
