/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * LEFT TO DO:
 * - Make a comma-separated list for the CHANNEL MAPPING & COUPLED/UNCOUPLED
 *   stream count.
 * - Plumb the CHANNEL MAPPING down to OPUS-C-code
 * - Make a DECODER (also with the CHANNEL MAPPING and COUPLED/UNCOUPLED stream
 *   count).
 * - Check what duplicated code can be SHARED between THIS and the MONO/STEREO
 *   ordinary OPUS codec.
 * - RENAME this file into "..._impl.cc". Same with the header.
 * - WRITE TESTS
 * - CHANGE the 4-channel test into a 6-channel one. Maybe.
 */

#include "modules/audio_coding/codecs/opus/audio_encoder_multi_channel_opus_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {

namespace {

// Recommended bitrates: TODO(aleloi) adapt for MULTI-OPUS
// 8-12 kb/s for NB speech,
// 16-20 kb/s for WB speech,
// 28-40 kb/s for FB speech,
// 48-64 kb/s for FB mono music, and
// 64-128 kb/s for FB stereo music.
// The current implementation applies the following values to mono signals,
// and multiplies them by 2 for stereo.
constexpr int kOpusBitrateNbBps = 12000;
constexpr int kOpusBitrateWbBps = 20000;
constexpr int kOpusBitrateFbBps = 32000;

constexpr int kDefaultMaxPlaybackRate = 48000;
// These two lists must be sorted from low to high
#if WEBRTC_OPUS_SUPPORT_120MS_PTIME
constexpr int kOpusSupportedFrameLengths[] = {10, 20, 40, 60, 120};
#else
constexpr int kOpusSupportedFrameLengths[] = {10, 20, 40, 60};
#endif

absl::optional<std::string> GetFormatParameter(const SdpAudioFormat& format,
                                               const std::string& param) {
  auto it = format.parameters.find(param);
  if (it == format.parameters.end())
    return absl::nullopt;

  return it->second;
}

template <typename T>
absl::optional<T> GetFormatParameter(const SdpAudioFormat& format,
                                     const std::string& param) {
  return rtc::StringToNumber<T>(GetFormatParameter(format, param).value_or(""));
}

template <>
absl::optional<std::vector<int>> GetFormatParameter(
    const SdpAudioFormat& format,
    const std::string& param) {
  std::string comma_separated_list =
      GetFormatParameter(format, param).value_or("");
  std::vector<std::string> splitted_list =
      absl::StrSplit(comma_separated_list, ",");
  std::vector<int> result;

  for (const auto s : splitted_list) {
    auto conv = rtc::StringToNumber<int>(s);
    if (!conv.has_value()) {
      return absl::nullopt;
    }
    result.push_back(*conv);
  }
  return result;
}

int GetBitrateBps(const AudioEncoderMultiChannelOpusConfig& config) {
  RTC_DCHECK(config.IsOk());
  return *config.single_stream_config.bitrate_bps;
}
int GetMaxPlaybackRate(const SdpAudioFormat& format) {
  const auto param = GetFormatParameter<int>(format, "maxplaybackrate");
  if (param && *param >= 8000) {
    return std::min(*param, kDefaultMaxPlaybackRate);
  }
  return kDefaultMaxPlaybackRate;
}

int GetFrameSizeMs(const SdpAudioFormat& format) {
  const auto ptime = GetFormatParameter<int>(format, "ptime");
  if (ptime.has_value()) {
    // Pick the next highest supported frame length from
    // kOpusSupportedFrameLengths.
    for (const int supported_frame_length : kOpusSupportedFrameLengths) {
      if (supported_frame_length >= *ptime) {
        return supported_frame_length;
      }
    }
    // If none was found, return the largest supported frame length.
    return *(std::end(kOpusSupportedFrameLengths) - 1);
  }

  return AudioEncoderOpusConfig::kDefaultFrameSizeMs;
}

// absl::optional<std::vector<int>> GetStreamMapping(const SdpAudioFormat&
// format) {
//   auto result = GetFormatParameter<std::vector<int>>(format,
//   "channel_mapping"); if (!result.has_value()) {
//     // Then we can't construct a codec. How do we signal that? TODO(aleloi)
//     fix. return std::vector<int>();
//   }

//   // Check that it makes sense. We don't really have to, because the opus
//   // functions do that. But it's better to detect errors early. TODO(aleloi)
//   // fix!
//   return *result;
// }

int CalculateDefaultBitrate(int max_playback_rate, size_t num_channels) {
  const int bitrate = [&] {
    if (max_playback_rate <= 8000) {
      return kOpusBitrateNbBps * rtc::dchecked_cast<int>(num_channels);
    } else if (max_playback_rate <= 16000) {
      return kOpusBitrateWbBps * rtc::dchecked_cast<int>(num_channels);
    } else {
      return kOpusBitrateFbBps * rtc::dchecked_cast<int>(num_channels);
    }
  }();
  RTC_DCHECK_GE(bitrate, AudioEncoderOpusConfig::kMinBitrateBps);
  RTC_DCHECK_LE(bitrate, AudioEncoderOpusConfig::kMaxBitrateBps);
  return bitrate;
}

// Get the maxaveragebitrate parameter in string-form, so we can properly figure
// out how invalid it is and accurately log invalid values.
int CalculateBitrate(int max_playback_rate_hz,
                     size_t num_channels,
                     absl::optional<std::string> bitrate_param) {
  const int default_bitrate =
      CalculateDefaultBitrate(max_playback_rate_hz, num_channels);

  if (bitrate_param) {
    const auto bitrate = rtc::StringToNumber<int>(*bitrate_param);
    if (bitrate) {
      const int chosen_bitrate =
          std::max(AudioEncoderOpusConfig::kMinBitrateBps,
                   std::min(*bitrate, AudioEncoderOpusConfig::kMaxBitrateBps));
      if (bitrate != chosen_bitrate) {
        RTC_LOG(LS_WARNING) << "Invalid maxaveragebitrate " << *bitrate
                            << " clamped to " << chosen_bitrate;
      }
      return chosen_bitrate;
    }
    RTC_LOG(LS_WARNING) << "Invalid maxaveragebitrate \"" << *bitrate_param
                        << "\" replaced by default bitrate " << default_bitrate;
  }

  return default_bitrate;
}

}  // namespace

std::unique_ptr<AudioEncoder>
AudioEncoderMultiChannelOpusImpl::MakeAudioEncoder(
    const AudioEncoderMultiChannelOpusConfig& config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  return absl::make_unique<AudioEncoderMultiChannelOpusImpl>(config,
                                                             payload_type);
}

// AudioEncoderMultiChannelOpusImpl::AudioEncoderMultiChannelOpusImpl(int
// payload_type, const SdpAudioFormat& format)
//     : AudioEncoderMultiChannelOpusImpl(*SdpToConfig(format), payload_type) {}

AudioEncoderMultiChannelOpusImpl::AudioEncoderMultiChannelOpusImpl(
    const AudioEncoderMultiChannelOpusConfig& config,
    int payload_type)
    : payload_type_(
          payload_type),  // we prbl need this one
                          // bitrate_changed_(true), // Do we need this?
                          // packet_loss_rate_(0.0), // Same here.
      inst_(nullptr) {
  // consecutive_dtx_frames_(0) // Do we track dtx frames? They don't work
  // anyway...
  RTC_DCHECK(0 <= payload_type && payload_type <= 127);

  // Sanity check of the redundant payload type field that we want to get rid
  // of. See https://bugs.chromium.org/p/webrtc/issues/detail?id=7847
  RTC_CHECK(config.single_stream_config.payload_type == -1 ||
            config.single_stream_config.payload_type == payload_type);

  RTC_CHECK(RecreateEncoderInstance(config));

  // What about this one? Maybe leave it in? Maybe not for the 1st version.
  // SetProjectedPacketLossRate(packet_loss_rate_);
}

AudioEncoderMultiChannelOpusImpl::~AudioEncoderMultiChannelOpusImpl() = default;

size_t AudioEncoderMultiChannelOpusImpl::SufficientOutputBufferSize() const {
  // Calculate the number of bytes we expect the encoder to produce,
  // then multiply by two to give a wide margin for error.
  const size_t bytes_per_millisecond =
      static_cast<size_t>(GetBitrateBps(config_) / (1000 * 8) + 1);
  const size_t approx_encoded_bytes =
      Num10msFramesPerPacket() * 10 * bytes_per_millisecond;
  return 2 * approx_encoded_bytes;
}

void AudioEncoderMultiChannelOpusImpl::Reset() {
  RTC_CHECK(RecreateEncoderInstance(config_));
}

// If the given config is OK, recreate the Opus encoder instance with those
// settings, save the config, and return true. Otherwise, do nothing and return
// false.
bool AudioEncoderMultiChannelOpusImpl::RecreateEncoderInstance(
    const AudioEncoderMultiChannelOpusConfig& config) {
  if (!config.IsOk())
    return false;
  config_ = config;
  if (inst_)
    RTC_CHECK_EQ(0, WebRtcOpus_EncoderFree(inst_));
  input_buffer_.clear();
  input_buffer_.reserve(Num10msFramesPerPacket() * SamplesPer10msFrame());
  RTC_CHECK_EQ(0, WebRtcOpus_MultistreamEncoderCreate(
                      &inst_, config.single_stream_config.num_channels,
                      config.single_stream_config.application ==
                              AudioEncoderOpusConfig::ApplicationMode::kVoip
                          ? 0
                          : 1,
                      config.coupled_streams, config.channel_mapping.data()));
  const int bitrate = GetBitrateBps(config);
  RTC_CHECK_EQ(0, WebRtcOpus_SetBitRate(inst_, bitrate));
  RTC_LOG(LS_INFO) << "Set Opus bitrate to " << bitrate << " bps.";
  if (config.single_stream_config.fec_enabled) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableFec(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableFec(inst_));
  }
  RTC_CHECK_EQ(0, WebRtcOpus_SetMaxPlaybackRate(
                      inst_, config.single_stream_config.max_playback_rate_hz));
  // Use the default complexity if the start bitrate is within the hysteresis
  // window.
  // complexity_ =
  // GetNewComplexity(config).value_or(config.single_stream_config.complexity);

  // Use the DEFAULT complexity.
  RTC_CHECK_EQ(
      0, WebRtcOpus_SetComplexity(inst_, AudioEncoderOpusConfig().complexity));

  // bitrate_changed_ = true;
  if (config.single_stream_config.dtx_enabled) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableDtx(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableDtx(inst_));
  }

  // Havn't set PCL-rate in the lower-level tests. 0 should prbl be the default?
  // RTC_CHECK_EQ(0,
  //              WebRtcOpus_SetPacketLossRate(
  //                  inst_, static_cast<int32_t>(packet_loss_rate_ * 100 +
  //                  .5)));
  if (config.single_stream_config.cbr_enabled) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableCbr(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableCbr(inst_));
  }
  num_channels_to_encode_ = NumChannels();
  next_frame_length_ms_ = config_.single_stream_config.frame_size_ms;
  return true;
}

absl::optional<AudioEncoderMultiChannelOpusConfig>
AudioEncoderMultiChannelOpusImpl::SdpToConfig(const SdpAudioFormat& format) {
  // TODO(aleloi): make the required changes. Make a comma-separated list for
  // the STREAM/CHANNEL MAPPING and COUPLED/UNCOUPLED STREAMS.
  if (!absl::EqualsIgnoreCase(format.name, "multiopus") ||
      format.clockrate_hz != 48000 ||
      (format.num_channels != 4 && format.num_channels != 6 &&
       format.num_channels != 8)) {
    return absl::nullopt;
  }

  AudioEncoderMultiChannelOpusConfig config;
  config.single_stream_config.num_channels =
      format.num_channels;  // GetChannelCount(format);
  config.single_stream_config.frame_size_ms = GetFrameSizeMs(format);
  config.single_stream_config.max_playback_rate_hz = GetMaxPlaybackRate(format);
  config.single_stream_config.fec_enabled =
      (GetFormatParameter(format, "useinbandfec") == "1");
  config.single_stream_config.dtx_enabled =
      (GetFormatParameter(format, "usedtx") == "1");
  config.single_stream_config.cbr_enabled =
      (GetFormatParameter(format, "cbr") == "1");
  config.single_stream_config.bitrate_bps =
      CalculateBitrate(config.single_stream_config.max_playback_rate_hz,
                       config.single_stream_config.num_channels,
                       GetFormatParameter(format, "maxaveragebitrate"));
  config.single_stream_config.application =
      config.single_stream_config.num_channels == 1
          ? AudioEncoderOpusConfig::ApplicationMode::kVoip
          : AudioEncoderOpusConfig::ApplicationMode::kAudio;

  // // For now, minptime and maxptime are only used with ANA. If ptime is
  // outside
  // // of this range, it will get adjusted once ANA takes hold. Ideally, we'd
  // know
  // // if ANA was to be used when setting up the config, and adjust
  // accordingly. const int min_frame_length_ms =
  //     GetFormatParameter<int>(format,
  //     "minptime").value_or(kMinANAFrameLength);
  // const int max_frame_length_ms =
  //     GetFormatParameter<int>(format,
  //     "maxptime").value_or(kMaxANAFrameLength);

  // FindSupportedFrameLengths(min_frame_length_ms, max_frame_length_ms,
  //                           &config.single_stream_config.supported_frame_lengths_ms);

  // Haven't tested this, does it work? It SHOULD.
  config.single_stream_config.supported_frame_lengths_ms.clear();
  std::copy(std::begin(kOpusSupportedFrameLengths),
            std::end(kOpusSupportedFrameLengths),
            std::back_inserter(
                config.single_stream_config.supported_frame_lengths_ms));

  auto coupled_streams = GetFormatParameter<int>(format, "coupled_streams");
  if (!coupled_streams.has_value()) {
    return absl::nullopt;
  }
  config.coupled_streams = *coupled_streams;

  auto channel_mapping =
      GetFormatParameter<std::vector<int>>(format, "channel_mapping");
  if (!channel_mapping.has_value()) {
    return absl::nullopt;
  }
  // Convert to 'unsigned char':
  config.channel_mapping.clear();
  std::copy(channel_mapping->begin(), channel_mapping->end(),
            std::back_inserter(config.channel_mapping));

  RTC_DCHECK(config.IsOk());
  return config;
}

void AudioEncoderMultiChannelOpusImpl::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  // We advertise support here, but add the codec as "NonAdvertised", so it
  // doesn't matter. TODO(aleloi): add code that handles the STREAM MAPPING and
  // #COUPLED/UNCOUPLED STREAMS.
  {
    const SdpAudioFormat fmt = {
        "opus", 48000, 6, {{"minptime", "10"}, {"useinbandfec", "1"}}};
    const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
    specs->push_back({fmt, info});
  }
  {
    const SdpAudioFormat fmt = {
        "opus", 48000, 8, {{"minptime", "10"}, {"useinbandfec", "1"}}};
    const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
    specs->push_back({fmt, info});
  }
}

AudioCodecInfo AudioEncoderMultiChannelOpusImpl::QueryAudioEncoder(
    const AudioEncoderMultiChannelOpusConfig& config) {
  RTC_DCHECK(config.IsOk());
  AudioCodecInfo info(48000, config.single_stream_config.num_channels,
                      *config.single_stream_config.bitrate_bps,
                      AudioEncoderOpusConfig::kMinBitrateBps,
                      AudioEncoderOpusConfig::kMaxBitrateBps);
  info.allow_comfort_noise = false;
  info.supports_network_adaption = false;
  return info;
}

// Overriden util-methods. Check which ones are used.
size_t AudioEncoderMultiChannelOpusImpl::Num10msFramesPerPacket() const {
  return static_cast<size_t>(
      rtc::CheckedDivExact(config_.single_stream_config.frame_size_ms, 10));
}
size_t AudioEncoderMultiChannelOpusImpl::SamplesPer10msFrame() const {
  return rtc::CheckedDivExact(48000, 100) *
         config_.single_stream_config.num_channels;
}
int AudioEncoderMultiChannelOpusImpl::SampleRateHz() const {
  return 48000;
}
size_t AudioEncoderMultiChannelOpusImpl::NumChannels() const {
  return config_.single_stream_config.num_channels;
}
size_t AudioEncoderMultiChannelOpusImpl::Num10MsFramesInNextPacket() const {
  return Num10msFramesPerPacket();
}
size_t AudioEncoderMultiChannelOpusImpl::Max10MsFramesInAPacket() const {
  return Num10msFramesPerPacket();
}
int AudioEncoderMultiChannelOpusImpl::GetTargetBitrate() const {
  return GetBitrateBps(config_);
}

AudioEncoder::EncodedInfo AudioEncoderMultiChannelOpusImpl::EncodeImpl(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    rtc::Buffer* encoded) {
  // ANA stuff. Has to do with 'bitrate smoother'.
  // MaybeUpdateUplinkBandwidth();

  // Do we need to keep track of the time stamp? Why do we need an 'input
  // buffer'? Can we feed the 'audio' directly into Opus? Why is there a lambda?
  // ANSWER: YES, we need the buffer. The OPUS API,
  // https://www.opus-codec.org/docs/opus_api-1.1.2/group__opus__encoder.html#gad2d6bf6a9ffb6674879d7605ed073e25
  // says the input should be exactly ONE FRAME. We have a field for that in the
  // config struct, but we don't need to tell OPUS if we change the frame
  // size. It can code any of the supported frame sizes in any order (they can
  // change between one opus_encode() and the next).
  if (input_buffer_.empty())
    first_timestamp_in_buffer_ = rtp_timestamp;

  input_buffer_.insert(input_buffer_.end(), audio.cbegin(), audio.cend());
  if (input_buffer_.size() <
      (Num10msFramesPerPacket() * SamplesPer10msFrame())) {
    return EncodedInfo();
  }
  RTC_CHECK_EQ(input_buffer_.size(),
               Num10msFramesPerPacket() * SamplesPer10msFrame());

  const size_t max_encoded_bytes = SufficientOutputBufferSize();
  EncodedInfo info;
  info.encoded_bytes = encoded->AppendData(
      max_encoded_bytes, [&](rtc::ArrayView<uint8_t> encoded) {
        int status = WebRtcOpus_Encode(
            inst_, &input_buffer_[0],
            rtc::CheckedDivExact(input_buffer_.size(),
                                 config_.single_stream_config.num_channels),
            rtc::saturated_cast<int16_t>(max_encoded_bytes), encoded.data());

        RTC_CHECK_GE(status, 0);  // Fails only if fed invalid data.

        return static_cast<size_t>(status);
      });
  input_buffer_.clear();

  // Doesn't work anyway.
  // bool dtx_frame = (info.encoded_bytes <= 2);

  // Will use new packet size for next encoding.
  config_.single_stream_config.frame_size_ms = next_frame_length_ms_;

  // if (adjust_bandwidth_ && bitrate_changed_) {
  //   const auto bandwidth = GetNewBandwidth(config_, inst_);
  //   if (bandwidth) {
  //     RTC_CHECK_EQ(0, WebRtcOpus_SetBandwidth(inst_, *bandwidth));
  //   }
  //   bitrate_changed_ = false;
  // }

  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = payload_type_;
  info.send_even_if_empty = true;  // Allows Opus to send empty packets.
  // After 20 DTX frames (MAX_CONSECUTIVE_DTX) Opus will send a frame
  // coding the background noise. Avoid flagging this frame as speech
  // (even though there is a probability of the frame being speech).
  info.speech = true;  // !dtx_frame && (consecutive_dtx_frames_ != 20);
  info.encoder_type = CodecType::kOpus;

  // Increase or reset DTX counter.
  // consecutive_dtx_frames_ = (dtx_frame) ? (consecutive_dtx_frames_ + 1) :
  // (0);

  return info;
}

}  // namespace webrtc
