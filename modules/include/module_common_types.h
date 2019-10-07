/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
#define MODULES_INCLUDE_MODULE_COMMON_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/rtp_headers.h"
#include "api/video/video_frame_type.h"
#include "modules/include/module_common_types_public.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RTPFragmentationHeader {
 public:
  RTPFragmentationHeader() = default;
  RTPFragmentationHeader(size_t num_fragments) : fragments_(num_fragments) {}
  RTPFragmentationHeader(const RTPFragmentationHeader&) = delete;
  RTPFragmentationHeader(RTPFragmentationHeader&& other) = default;
  RTPFragmentationHeader& operator=(const RTPFragmentationHeader& other) =
      delete;
  RTPFragmentationHeader& operator=(RTPFragmentationHeader&& other) = default;
  ~RTPFragmentationHeader() = default;

  friend void swap(RTPFragmentationHeader& a, RTPFragmentationHeader& b) {
    swap(a.fragments_, b.fragments_);
  }
  friend bool operator==(const RTPFragmentationHeader& lhs,
                         const RTPFragmentationHeader& rhs) {
    return lhs.fragments_ == rhs.fragments_;
  }

  void CopyFrom(const RTPFragmentationHeader& src) {
    fragments_ = src.fragments_;
  }
  void VerifyAndAllocateFragmentationHeader(size_t size) {
    fragments_.resize(size);
  }

  void Set(size_t index, size_t offset, size_t length) {
    fragments_[index] = {offset, length};
  }

  size_t Size() const { return fragments_.size(); }

  size_t Offset(size_t index) const { return fragments_[index].offset; }
  size_t Length(size_t index) const { return fragments_[index].length; }

 private:
  struct Fragment {
    friend bool operator==(const Fragment& lhs, const Fragment& rhs) {
      return lhs.offset == rhs.offset && lhs.length == rhs.length;
    }
    size_t offset = 0;
    size_t length = 0;
  };
  std::vector<Fragment> fragments_;
};

// Interface used by the CallStats class to distribute call statistics.
// Callbacks will be triggered as soon as the class has been registered to a
// CallStats object using RegisterStatsObserver.
class CallStatsObserver {
 public:
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) = 0;

  virtual ~CallStatsObserver() {}
};

// Interface used by NackModule and JitterBuffer.
class NackSender {
 public:
  // If |buffering_allowed|, other feedback messages (e.g. key frame requests)
  // may be added to the same outgoing feedback message. In that case, it's up
  // to the user of the interface to ensure that when all buffer-able messages
  // have been added, the feedback message is triggered.
  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers,
                        bool buffering_allowed) = 0;

 protected:
  virtual ~NackSender() {}
};

// Interface used by NackModule and JitterBuffer.
class KeyFrameRequestSender {
 public:
  virtual void RequestKeyFrame() = 0;

 protected:
  virtual ~KeyFrameRequestSender() {}
};

// Interface used by LossNotificationController to communicate to RtpRtcp.
class LossNotificationSender {
 public:
  virtual ~LossNotificationSender() {}

  virtual void SendLossNotification(uint16_t last_decoded_seq_num,
                                    uint16_t last_received_seq_num,
                                    bool decodability_flag,
                                    bool buffering_allowed) = 0;
};

}  // namespace webrtc

#endif  // MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
