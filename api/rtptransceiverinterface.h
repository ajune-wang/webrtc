/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpTransceivers.
// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver

#ifndef API_RTPTRANSCEIVERINTERFACE_H_
#define API_RTPTRANSCEIVERINTERFACE_H_

#include <string>

#include "api/optional.h"
#include "api/rtpreceiverinterface.h"
#include "api/rtpsenderinterface.h"
#include "rtc_base/refcount.h"

namespace webrtc {

enum class RtpTransceiverDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly,
  kInactive
};

class RtpTransceiverInterface : public rtc::RefCountInterface {
 public:
  // The mid attribute is the mid negotiated and present in the local and
  // remote descriptions as defined in [JSEP] (section 5.2.1. and
  // section 5.3.1.). Before negotiation is complete, the mid value may be null.
  // After rollbacks, the value may change from a non-null value to null.
  virtual rtc::Optional<std::string> mid() const = 0;

  virtual rtc::scoped_refptr<RtpSenderInterface> sender() const = 0;

  virtual rtc::scoped_refptr<RtpReceiverInterface> receiver() const = 0;

  virtual bool stopped() const = 0;

  virtual RtpTransceiverDirection direction() const = 0;

  virtual void set_direction(RtpTransceiverDirection) = 0;

  virtual rtc::Optional<RtpTransceiverDirection> current_direction() const = 0;

  virtual void Stop() = 0;

  virtual void SetCodecPreferences(
      rtc::ArrayView<RtpCodecCapability> codecs) = 0;

 protected:
  virtual ~RtpTransceiverInterface() = default;
};

}  // namespace webrtc

#endif  // API_RTPTRANSCEIVERINTERFACE_H_
