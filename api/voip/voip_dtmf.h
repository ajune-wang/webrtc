/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_DTMF_H_
#define API_VOIP_VOIP_DTMF_H_

#include "api/voip/voip_base.h"

namespace webrtc {

enum class DtmfEvent : uint8_t {
  kDigitZero = 0,
  kDigitOne,
  kDigitTwo,
  kDigitThree,
  kDigitFour,
  kDigitFive,
  kDigitSix,
  kDigitSeven,
  kDigitEight,
  kDigitNine,
  kAsterisk,
  kHash,
  kLetterA,
  kLetterB,
  kLetterC,
  kLetterD
};

// VoipDtmf interface provides DTMF related interfaces such
// as sending DTMF events to the remote endpoint.
class VoipDtmf {
 public:
  // Register the payload type and sample rate for DTMF (RFC 4733) payload.
  virtual void RegisterTelephoneEventType(ChannelId channel_id,
                                          int rtp_payload_type,
                                          int sample_rate_hz) = 0;

  // Send DTMF named event as specified by
  // https://tools.ietf.org/html/rfc4733#section-3.2
  // |duration_ms| specifies the duration of DTMF packets that will be emitted
  // in place of real RTP packets instead.
  // This will return true when requested dtmf event is successfully scheduled
  // otherwise false when the dtmf queue reached maximum of 20 events.
  virtual bool SendDtmfEvent(ChannelId channel_id,
                             DtmfEvent dtmf_event,
                             int duration_ms) = 0;

 protected:
  virtual ~VoipDtmf() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_DTMF_H_
