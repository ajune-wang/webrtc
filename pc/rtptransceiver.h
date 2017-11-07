/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpSenderInterface.
// An RtpSender associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef PC_RTPTRANSCEIVER_H_
#define PC_RTPTRANSCEIVER_H_

#include <vector>

#include "api/rtptransceiverinterface.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"

namespace webrtc {

class PeerConnection;

class RtpTransceiver final : public rtc::RefCountedObject<RtpTransceiverInterface> {
 public:
  RtpTransceiver(cricket::MediaType kind);
  ~RtpTransceiver() override;

  cricket::MediaType type() const;

  cricket::BaseChannel* channel() const;
  void set_channel(cricket::BaseChannel* channel);

  void AddSender(rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender);
  bool RemoveSender(RtpSenderInterface* sender);
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>> senders() const;

  void AddReceiver(rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>> receiver);
  bool RemoveReceiver(RtpReceiverInterface* receiver);
  std::vector<rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>> receivers() const;

  // RtpTransceiverInterface implementation.
  rtc::Optional<std::string> mid() const override;
  rtc::scoped_refptr<RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  RtpTransceiverDirection direction() const override;
  void set_direction(RtpTransceiverDirection new_direction) override;
  rtc::Optional<RtpTransceiverDirection> current_direction() const override;
  void Stop() override;
  void SetCodecPreferences(rtc::ArrayView<RtpCodecCapability> codecs) override;

 private:
  const cricket::MediaType type_;
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>> senders_;
  std::vector<rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>> receivers_;

  bool stopped_ = false;
  RtpTransceiverDirection direction_ = RtpTransceiverDirection::kInactive;
  rtc::Optional<RtpTransceiverDirection> current_direction_;
  rtc::Optional<std::string> mid_;

  cricket::BaseChannel* channel_ = nullptr;
};

BEGIN_SIGNALING_PROXY_MAP(RtpTransceiver)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(rtc::Optional<std::string>, mid);
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpSenderInterface>, sender);
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpReceiverInterface>, receiver);
PROXY_CONSTMETHOD0(bool, stopped);
PROXY_CONSTMETHOD0(RtpTransceiverDirection, direction);
PROXY_METHOD1(void, set_direction, RtpTransceiverDirection);
PROXY_CONSTMETHOD0(rtc::Optional<RtpTransceiverDirection>, current_direction);
PROXY_METHOD0(void, Stop);
PROXY_METHOD1(void, SetCodecPreferences, rtc::ArrayView<RtpCodecCapability>);
END_PROXY_MAP();

}  // namespace webrtc

#endif  // PC_RTPTRANSCEIVER_H_
