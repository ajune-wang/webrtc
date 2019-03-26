/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_transceiver.h"

#include <string>

#include "absl/algorithm/container.h"
#include "pc/channel_manager.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_parameters_conversion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpTransceiver::RtpTransceiver(cricket::MediaType media_type)
    : unified_plan_(false), media_type_(media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
}

RtpTransceiver::RtpTransceiver(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver,
    cricket::ChannelManager* channel_manager)
    : unified_plan_(true),
      media_type_(sender->media_type()),
      channel_manager_(channel_manager) {
  RTC_DCHECK(media_type_ == cricket::MEDIA_TYPE_AUDIO ||
             media_type_ == cricket::MEDIA_TYPE_VIDEO);
  RTC_DCHECK_EQ(sender->media_type(), receiver->media_type());
  senders_.push_back(sender);
  receivers_.push_back(receiver);
}

RtpTransceiver::~RtpTransceiver() {
  Stop();
}

void RtpTransceiver::SetChannel(cricket::ChannelInterface* channel) {
  // Cannot set a non-null channel on a stopped transceiver.
  if (stopped_ && channel) {
    return;
  }

  if (channel) {
    RTC_DCHECK_EQ(media_type(), channel->media_type());
  }

  if (channel_) {
    channel_->SignalFirstPacketReceived().disconnect(this);
  }

  channel_ = channel;

  if (channel_) {
    channel_->SignalFirstPacketReceived().connect(
        this, &RtpTransceiver::OnFirstPacketReceived);
  }

  for (const auto& sender : senders_) {
    sender->internal()->SetMediaChannel(channel_ ? channel_->media_channel()
                                                 : nullptr);
  }

  for (const auto& receiver : receivers_) {
    if (!channel_) {
      receiver->internal()->Stop();
    }

    receiver->internal()->SetMediaChannel(channel_ ? channel_->media_channel()
                                                   : nullptr);
  }
}

void RtpTransceiver::AddSender(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender) {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(sender);
  RTC_DCHECK_EQ(media_type(), sender->media_type());
  RTC_DCHECK(!absl::c_linear_search(senders_, sender));
  senders_.push_back(sender);
}

bool RtpTransceiver::RemoveSender(RtpSenderInterface* sender) {
  RTC_DCHECK(!unified_plan_);
  if (sender) {
    RTC_DCHECK_EQ(media_type(), sender->media_type());
  }
  auto it = absl::c_find(senders_, sender);
  if (it == senders_.end()) {
    return false;
  }
  (*it)->internal()->Stop();
  senders_.erase(it);
  return true;
}

void RtpTransceiver::AddReceiver(
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver) {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(receiver);
  RTC_DCHECK_EQ(media_type(), receiver->media_type());
  RTC_DCHECK(!absl::c_linear_search(receivers_, receiver));
  receivers_.push_back(receiver);
}

bool RtpTransceiver::RemoveReceiver(RtpReceiverInterface* receiver) {
  RTC_DCHECK(!unified_plan_);
  if (receiver) {
    RTC_DCHECK_EQ(media_type(), receiver->media_type());
  }
  auto it = absl::c_find(receivers_, receiver);
  if (it == receivers_.end()) {
    return false;
  }
  (*it)->internal()->Stop();
  receivers_.erase(it);
  return true;
}

rtc::scoped_refptr<RtpSenderInternal> RtpTransceiver::sender_internal() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return senders_[0]->internal();
}

rtc::scoped_refptr<RtpReceiverInternal> RtpTransceiver::receiver_internal()
    const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return receivers_[0]->internal();
}

cricket::MediaType RtpTransceiver::media_type() const {
  return media_type_;
}

absl::optional<std::string> RtpTransceiver::mid() const {
  return mid_;
}

void RtpTransceiver::OnFirstPacketReceived(cricket::ChannelInterface*) {
  for (const auto& receiver : receivers_) {
    receiver->internal()->NotifyFirstPacketReceived();
  }
}

rtc::scoped_refptr<RtpSenderInterface> RtpTransceiver::sender() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return senders_[0];
}

rtc::scoped_refptr<RtpReceiverInterface> RtpTransceiver::receiver() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return receivers_[0];
}

void RtpTransceiver::set_current_direction(RtpTransceiverDirection direction) {
  RTC_LOG(LS_INFO) << "Changing transceiver (MID=" << mid_.value_or("<not set>")
                   << ") current direction from "
                   << (current_direction_ ? RtpTransceiverDirectionToString(
                                                *current_direction_)
                                          : "<not set>")
                   << " to " << RtpTransceiverDirectionToString(direction)
                   << ".";
  current_direction_ = direction;
  if (RtpTransceiverDirectionHasSend(*current_direction_)) {
    has_ever_been_used_to_send_ = true;
  }
}

void RtpTransceiver::set_fired_direction(RtpTransceiverDirection direction) {
  fired_direction_ = direction;
}

bool RtpTransceiver::stopped() const {
  return stopped_;
}

RtpTransceiverDirection RtpTransceiver::direction() const {
  return direction_;
}

void RtpTransceiver::SetDirection(RtpTransceiverDirection new_direction) {
  if (stopped()) {
    return;
  }
  if (new_direction == direction_) {
    return;
  }
  direction_ = new_direction;
  SignalNegotiationNeeded();
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::current_direction()
    const {
  return current_direction_;
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::fired_direction()
    const {
  return fired_direction_;
}

void RtpTransceiver::Stop() {
  for (const auto& sender : senders_) {
    sender->internal()->Stop();
  }
  for (const auto& receiver : receivers_) {
    receiver->internal()->Stop();
  }
  stopped_ = true;
  current_direction_ = absl::nullopt;
}

RTCError RtpTransceiver::SetCodecPreferences(
    rtc::ArrayView<RtpCodecCapability> codecs) {
  // When the list is empty, we reset the preferences
  if (codecs.empty()) {
    codec_preferences_.clear();
    return RTCError::OK();
  }

  // Remove duplicate codecs from the tail
  std::vector<RtpCodecCapability> deduplicated_codecs;
  absl::c_remove_copy_if(
      codecs,
      std::back_insert_iterator<std::vector<RtpCodecCapability>>(
          deduplicated_codecs),
      [&deduplicated_codecs](const RtpCodecCapability& codec) {
        return absl::c_find(deduplicated_codecs, codec) !=
               deduplicated_codecs.end();
      });

  codecs = deduplicated_codecs;

  if (media_type_ == cricket::MEDIA_TYPE_AUDIO) {
    std::vector<cricket::AudioCodec> audio_codecs;
    switch (direction_) {
      case webrtc::RtpTransceiverDirection::kRecvOnly:
        channel_manager_->GetSupportedAudioReceiveCodecs(&audio_codecs);
        break;
      case webrtc::RtpTransceiverDirection::kSendOnly:
        channel_manager_->GetSupportedAudioSendCodecs(&audio_codecs);
        break;
      case webrtc::RtpTransceiverDirection::kSendRecv: {
        std::vector<cricket::AudioCodec> recv_codecs, send_codecs;
        channel_manager_->GetSupportedAudioReceiveCodecs(&recv_codecs);
        channel_manager_->GetSupportedAudioSendCodecs(&send_codecs);

        // Compute the intersection of recv_codecs and send_codecs
        for (const auto& recv_codec : recv_codecs) {
          if (std::find(send_codecs.begin(), send_codecs.end(), recv_codec) !=
              send_codecs.end()) {
            audio_codecs.push_back(recv_codec);
          }
        }
        break;
      }
      case webrtc::RtpTransceiverDirection::kInactive:
        break;
    }

    // Validate codecs
    for (auto& codec_preference : codecs) {
      if (absl::c_find_if(audio_codecs, [&codec_preference](
                                            const cricket::AudioCodec& codec) {
            webrtc::RtpCodecParameters codec_parameters =
                codec.ToCodecParameters();
            return codec_parameters.name == codec_preference.name &&
                   codec_parameters.kind == codec_preference.kind &&
                   codec_parameters.num_channels ==
                       codec_preference.num_channels &&
                   codec_parameters.clock_rate == codec_preference.clock_rate &&
                   codec_parameters.parameters == codec_preference.parameters;
          }) == audio_codecs.end()) {
        return RTCError(
            RTCErrorType::INVALID_MODIFICATION,
            std::string(
                "Invalid codec preferences: invalid codec with name \"") +
                codec_preference.name + "\".");
      }
    }
  } else if (media_type_ == cricket::MEDIA_TYPE_VIDEO) {
    std::vector<cricket::VideoCodec> supported_video_codecs;
    // Video codecs are both for the receive and send side, so no need to check
    // the transceiver direction
    channel_manager_->GetSupportedVideoCodecs(&supported_video_codecs);

    // Validate codecs
    for (auto& codec_preference : codecs) {
      if (absl::c_find_if(
              supported_video_codecs,
              [&codec_preference](const cricket::VideoCodec& codec) {
                webrtc::RtpCodecParameters codec_parameters =
                    codec.ToCodecParameters();

                bool isRtx = codec_preference.name == "rtx";

                return codec_parameters.name == codec_preference.name &&
                       codec_parameters.kind == codec_preference.kind &&
                       (isRtx || (codec_parameters.num_channels ==
                                      codec_preference.num_channels &&
                                  codec_parameters.clock_rate ==
                                      codec_preference.clock_rate &&
                                  codec_parameters.parameters ==
                                      codec_preference.parameters));
              }) == supported_video_codecs.end()) {
        return RTCError(
            RTCErrorType::INVALID_MODIFICATION,
            std::string(
                "Invalid codec preferences: invalid codec with name \"") +
                codec_preference.name + "\".");
      }
    }
  }

  // Check we have a real codec (not just rtx, red or fec)
  if (!absl::c_any_of(codecs, [](const RtpCodecCapability& codec) {
        return !(codec.name == cricket::kRtxCodecName ||
                 codec.name == cricket::kRedCodecName ||
                 codec.name == cricket::kUlpfecCodecName);
      })) {
    return RTCError(RTCErrorType::INVALID_MODIFICATION,
                    "Invalid codec preferences: codec list must have a non "
                    "RTX, RED or FEC entry.");
  }

  codec_preferences_ =
      std::vector<RtpCodecCapability>(codecs.begin(), codecs.end());

  return RTCError::OK();
}

}  // namespace webrtc
