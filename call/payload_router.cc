/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/payload_router.h"

#include "call/rtp_transport_controller_send_interface.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
static const int kMinSendSidePacketHistorySize = 600;

std::vector<std::unique_ptr<RtpRtcp>> CreateRtpRtcpModules(
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& protected_media_ssrcs,
    const Rtcp& rtcp_config,
    Transport* send_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    RtpTransportControllerSendInterface* transport,
    RtcpRttStats* rtt_stats,
    FlexfecSender* flexfec_sender,
    BitrateStatisticsObserver* bitrate_observer,
    FrameCountObserver* frame_count_observer,
    RtcpPacketTypeCounterObserver* rtcp_type_observer,
    SendSideDelayObserver* send_delay_observer,
    SendPacketObserver* send_packet_observer,
    RtcEventLog* event_log,
    RateLimiter* retransmission_rate_limiter,
    OverheadObserver* overhead_observer,
    RtpKeepAliveConfig keepalive_config) {
  RTC_DCHECK_GT(ssrcs.size(), 0);
  RtpRtcp::Configuration configuration;
  configuration.audio = false;
  configuration.receiver_only = false;
  configuration.outgoing_transport = send_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.transport_feedback_callback =
      transport->transport_feedback_observer();
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer = rtcp_type_observer;
  configuration.paced_sender = transport->packet_sender();
  configuration.transport_sequence_number_allocator =
      transport->packet_router();
  configuration.send_bitrate_observer = bitrate_observer;
  configuration.send_frame_count_observer = frame_count_observer;
  configuration.send_side_delay_observer = send_delay_observer;
  configuration.send_packet_observer = send_packet_observer;
  configuration.event_log = event_log;
  configuration.retransmission_rate_limiter = retransmission_rate_limiter;
  configuration.overhead_observer = overhead_observer;
  configuration.keepalive_config = keepalive_config;
  configuration.rtcp_interval_config.video_interval_ms =
      rtcp_config.video_report_interval_ms;
  configuration.rtcp_interval_config.audio_interval_ms =
      rtcp_config.audio_report_interval_ms;
  std::vector<std::unique_ptr<RtpRtcp>> modules;
  const std::vector<uint32_t>& flexfec_protected_ssrcs = protected_media_ssrcs;
  for (uint32_t ssrc : ssrcs) {
    bool enable_flexfec = flexfec_sender != nullptr &&
                          std::find(flexfec_protected_ssrcs.begin(),
                                    flexfec_protected_ssrcs.end(),
                                    ssrc) != flexfec_protected_ssrcs.end();
    configuration.flexfec_sender = enable_flexfec ? flexfec_sender : nullptr;
    std::unique_ptr<RtpRtcp> rtp_rtcp =
        std::unique_ptr<RtpRtcp>(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
    modules.push_back(std::move(rtp_rtcp));
  }
  return modules;
}

// Map information from info into rtp.
void CopyCodecSpecific(const CodecSpecificInfo* info, RTPVideoHeader* rtp) {
  RTC_DCHECK(info);
  rtp->codec = info->codecType;
  switch (info->codecType) {
    case kVideoCodecVP8: {
      rtp->vp8().InitRTPVideoHeaderVP8();
      rtp->vp8().nonReference = info->codecSpecific.VP8.nonReference;
      rtp->vp8().temporalIdx = info->codecSpecific.VP8.temporalIdx;
      rtp->vp8().layerSync = info->codecSpecific.VP8.layerSync;
      rtp->vp8().keyIdx = info->codecSpecific.VP8.keyIdx;
      rtp->simulcastIdx = info->codecSpecific.VP8.simulcastIdx;
      return;
    }
    case kVideoCodecVP9: {
      rtp->vp9().InitRTPVideoHeaderVP9();
      rtp->vp9().inter_pic_predicted =
          info->codecSpecific.VP9.inter_pic_predicted;
      rtp->vp9().flexible_mode = info->codecSpecific.VP9.flexible_mode;
      rtp->vp9().ss_data_available = info->codecSpecific.VP9.ss_data_available;
      rtp->vp9().non_ref_for_inter_layer_pred =
          info->codecSpecific.VP9.non_ref_for_inter_layer_pred;
      rtp->vp9().temporal_idx = info->codecSpecific.VP9.temporal_idx;
      rtp->vp9().spatial_idx = info->codecSpecific.VP9.spatial_idx;
      rtp->vp9().temporal_up_switch =
          info->codecSpecific.VP9.temporal_up_switch;
      rtp->vp9().inter_layer_predicted =
          info->codecSpecific.VP9.inter_layer_predicted;
      rtp->vp9().gof_idx = info->codecSpecific.VP9.gof_idx;
      rtp->vp9().num_spatial_layers =
          info->codecSpecific.VP9.num_spatial_layers;

      if (info->codecSpecific.VP9.ss_data_available) {
        rtp->vp9().spatial_layer_resolution_present =
            info->codecSpecific.VP9.spatial_layer_resolution_present;
        if (info->codecSpecific.VP9.spatial_layer_resolution_present) {
          for (size_t i = 0; i < info->codecSpecific.VP9.num_spatial_layers;
               ++i) {
            rtp->vp9().width[i] = info->codecSpecific.VP9.width[i];
            rtp->vp9().height[i] = info->codecSpecific.VP9.height[i];
          }
        }
        rtp->vp9().gof.CopyGofInfoVP9(info->codecSpecific.VP9.gof);
      }

      rtp->vp9().num_ref_pics = info->codecSpecific.VP9.num_ref_pics;
      for (int i = 0; i < info->codecSpecific.VP9.num_ref_pics; ++i) {
        rtp->vp9().pid_diff[i] = info->codecSpecific.VP9.p_diff[i];
      }
      rtp->vp9().end_of_picture = info->codecSpecific.VP9.end_of_picture;
      return;
    }
    case kVideoCodecH264:
      rtp->h264().packetization_mode =
          info->codecSpecific.H264.packetization_mode;
      rtp->simulcastIdx = info->codecSpecific.H264.simulcast_idx;
      return;
    case kVideoCodecMultiplex:
    case kVideoCodecGeneric:
      rtp->codec = kVideoCodecGeneric;
      rtp->simulcastIdx = info->codecSpecific.generic.simulcast_idx;
      return;
    default:
      return;
  }
}

void SetVideoTiming(VideoSendTiming* timing, const EncodedImage& image) {
  if (image.timing_.flags == VideoSendTiming::TimingFrameFlags::kInvalid ||
      image.timing_.flags == VideoSendTiming::TimingFrameFlags::kNotTriggered) {
    timing->flags = VideoSendTiming::TimingFrameFlags::kInvalid;
    return;
  }

  timing->encode_start_delta_ms = VideoSendTiming::GetDeltaCappedMs(
      image.capture_time_ms_, image.timing_.encode_start_ms);
  timing->encode_finish_delta_ms = VideoSendTiming::GetDeltaCappedMs(
      image.capture_time_ms_, image.timing_.encode_finish_ms);
  timing->packetization_finish_delta_ms = 0;
  timing->pacer_exit_delta_ms = 0;
  timing->network_timestamp_delta_ms = 0;
  timing->network2_timestamp_delta_ms = 0;
  timing->flags = image.timing_.flags;
}

bool PayloadTypeSupportsSkippingFecPackets(const std::string& payload_name) {
  const VideoCodecType codecType = PayloadStringToCodecType(payload_name);
  if (codecType == kVideoCodecVP8 || codecType == kVideoCodecVP9) {
    return true;
  }
  return false;
}

// TODO(brandtr): Update this function when we support multistream protection.
std::unique_ptr<FlexfecSender> MaybeCreateFlexfecSender(
    const Rtp& rtp,
    const std::map<uint32_t, RtpState>& suspended_ssrcs) {
  if (rtp.flexfec.payload_type < 0) {
    return nullptr;
  }
  RTC_DCHECK_GE(rtp.flexfec.payload_type, 0);
  RTC_DCHECK_LE(rtp.flexfec.payload_type, 127);
  if (rtp.flexfec.ssrc == 0) {
    RTC_LOG(LS_WARNING) << "FlexFEC is enabled, but no FlexFEC SSRC given. "
                           "Therefore disabling FlexFEC.";
    return nullptr;
  }
  if (rtp.flexfec.protected_media_ssrcs.empty()) {
    RTC_LOG(LS_WARNING)
        << "FlexFEC is enabled, but no protected media SSRC given. "
           "Therefore disabling FlexFEC.";
    return nullptr;
  }

  if (rtp.flexfec.protected_media_ssrcs.size() > 1) {
    RTC_LOG(LS_WARNING)
        << "The supplied FlexfecConfig contained multiple protected "
           "media streams, but our implementation currently only "
           "supports protecting a single media stream. "
           "To avoid confusion, disabling FlexFEC completely.";
    return nullptr;
  }

  const RtpState* rtp_state = nullptr;
  auto it = suspended_ssrcs.find(rtp.flexfec.ssrc);
  if (it != suspended_ssrcs.end()) {
    rtp_state = &it->second;
  }

  RTC_DCHECK_EQ(1U, rtp.flexfec.protected_media_ssrcs.size());
  return absl::make_unique<FlexfecSender>(
      rtp.flexfec.payload_type, rtp.flexfec.ssrc,
      rtp.flexfec.protected_media_ssrcs[0], rtp.mid, rtp.extensions,
      RTPSender::FecExtensionSizes(), rtp_state, Clock::GetRealTimeClock());
}
}  // namespace

// State for setting picture id and tl0 pic idx, for VP8 and VP9
// TODO(nisse): Make these properties not codec specific.
class PayloadRouter::RtpPayloadParams final {
 public:
  RtpPayloadParams(const uint32_t ssrc, const RtpPayloadState* state)
      : ssrc_(ssrc) {
    Random random(rtc::TimeMicros());
    state_.picture_id =
        state ? state->picture_id : (random.Rand<int16_t>() & 0x7FFF);
    state_.tl0_pic_idx = state ? state->tl0_pic_idx : (random.Rand<uint8_t>());
  }
  ~RtpPayloadParams() {}

  void Set(RTPVideoHeader* rtp_video_header, bool first_frame_in_picture) {
    // Always set picture id. Set tl0_pic_idx iff temporal index is set.
    if (first_frame_in_picture) {
      state_.picture_id =
          (static_cast<uint16_t>(state_.picture_id) + 1) & 0x7FFF;
    }
    if (rtp_video_header->codec == kVideoCodecVP8) {
      rtp_video_header->vp8().pictureId = state_.picture_id;

      if (rtp_video_header->vp8().temporalIdx != kNoTemporalIdx) {
        if (rtp_video_header->vp8().temporalIdx == 0) {
          ++state_.tl0_pic_idx;
        }
        rtp_video_header->vp8().tl0PicIdx = state_.tl0_pic_idx;
      }
    }
    if (rtp_video_header->codec == kVideoCodecVP9) {
      rtp_video_header->vp9().picture_id = state_.picture_id;

      // Note that in the case that we have no temporal layers but we do have
      // spatial layers, packets will carry layering info with a temporal_idx of
      // zero, and we then have to set and increment tl0_pic_idx.
      if (rtp_video_header->vp9().temporal_idx != kNoTemporalIdx ||
          rtp_video_header->vp9().spatial_idx != kNoSpatialIdx) {
        if (first_frame_in_picture &&
            (rtp_video_header->vp9().temporal_idx == 0 ||
             rtp_video_header->vp9().temporal_idx == kNoTemporalIdx)) {
          ++state_.tl0_pic_idx;
        }
        rtp_video_header->vp9().tl0_pic_idx = state_.tl0_pic_idx;
      }
    }
  }

  uint32_t ssrc() const { return ssrc_; }

  RtpPayloadState state() const { return state_; }

 private:
  const uint32_t ssrc_;
  RtpPayloadState state_;
};

PayloadRouter::PayloadRouter(
    const std::vector<uint32_t>& ssrcs,
    std::map<uint32_t, RtpState> suspended_ssrcs,
    const std::map<uint32_t, RtpPayloadState>& states,
    const Rtp& rtp_config,
    const Rtcp& rtcp_config,
    Transport* send_transport,
    RtcpRttStats* rtcp_rtt_stats,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpStatisticsCallback* rtcp_stats,
    StreamDataCountersCallback* rtp_stats,
    RtpTransportControllerSendInterface* transport,
    BitrateStatisticsObserver* bitrate_observer,
    FrameCountObserver* frame_count_observer,
    RtcpPacketTypeCounterObserver* rtcp_type_observer,
    SendSideDelayObserver* send_delay_observer,
    SendPacketObserver* send_packet_observer,  // move inside RtpTransport
    RtcEventLog* event_log,
    RateLimiter* retransmission_limiter,  // move inside RtpTransport
    OverheadObserver* overhead_observer)
    : active_(false),
      module_process_thread_(nullptr),
      suspended_ssrcs_(std::move(suspended_ssrcs)),
      flexfec_sender_(MaybeCreateFlexfecSender(rtp_config, suspended_ssrcs_)),
      rtp_modules_(
          CreateRtpRtcpModules(ssrcs,
                               rtp_config.flexfec.protected_media_ssrcs,
                               rtcp_config,
                               send_transport,
                               intra_frame_callback,
                               transport->GetBandwidthObserver(),
                               transport,
                               rtcp_rtt_stats,
                               flexfec_sender_.get(),
                               bitrate_observer,
                               frame_count_observer,
                               rtcp_type_observer,
                               send_delay_observer,
                               send_packet_observer,
                               event_log,
                               retransmission_limiter,
                               overhead_observer,
                               transport->keepalive_config())),
      rtp_config_(rtp_config),
      transport_(transport) {
  RTC_DCHECK_EQ(ssrcs.size(), rtp_modules_.size());
  module_process_thread_checker_.DetachFromThread();
  // SSRCs are assumed to be sorted in the same order as |rtp_modules|.
  for (uint32_t ssrc : ssrcs) {
    // Restore state if it previously existed.
    const RtpPayloadState* state = nullptr;
    auto it = states.find(ssrc);
    if (it != states.end()) {
      state = &it->second;
    }
    params_.push_back(RtpPayloadParams(ssrc, state));
  }

  // RTP/RTCP initialization.

  // We add the highest spatial layer first to ensure it'll be prioritized
  // when sending padding, with the hope that the packet rate will be smaller,
  // and that it's more important to protect than the lower layers.
  for (auto& rtp_rtcp : rtp_modules_) {
    constexpr bool remb_candidate = true;
    transport->packet_router()->AddSendRtpModule(rtp_rtcp.get(),
                                                 remb_candidate);
  }

  for (size_t i = 0; i < rtp_config_.extensions.size(); ++i) {
    const std::string& extension = rtp_config_.extensions[i].uri;
    int id = rtp_config_.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(id, 1);
    RTC_DCHECK_LE(id, 14);
    RTC_DCHECK(RtpExtension::IsSupportedForVideo(extension));
    for (auto& rtp_rtcp : rtp_modules_) {
      RTC_CHECK_EQ(0, rtp_rtcp->RegisterSendRtpHeaderExtension(
                          StringToRtpExtensionType(extension), id));
    }
  }

  ConfigureProtection(rtp_config);
  ConfigureSsrcs(rtp_config);

  if (!rtp_config.mid.empty()) {
    for (auto& rtp_rtcp : rtp_modules_) {
      rtp_rtcp->SetMid(rtp_config.mid);
    }
  }

  // TODO(pbos): Should we set CNAME on all RTP modules?
  rtp_modules_.front()->SetCNAME(rtp_config.c_name.c_str());

  for (auto& rtp_rtcp : rtp_modules_) {
    rtp_rtcp->RegisterRtcpStatisticsCallback(rtcp_stats);
    rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(rtp_stats);
    rtp_rtcp->SetMaxRtpPacketSize(rtp_config.max_packet_size);
    rtp_rtcp->RegisterVideoSendPayload(rtp_config.payload_type,
                                       rtp_config.payload_name.c_str());
  }
}

PayloadRouter::~PayloadRouter() {
  for (auto& rtp_rtcp : rtp_modules_) {
    transport_->packet_router()->RemoveSendRtpModule(rtp_rtcp.get());
  }
}

void PayloadRouter::RegisterProcessThread(
    ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;

  for (auto& rtp_rtcp : rtp_modules_)
    module_process_thread_->RegisterModule(rtp_rtcp.get(), RTC_FROM_HERE);
}

void PayloadRouter::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  for (auto& rtp_rtcp : rtp_modules_)
    module_process_thread_->DeRegisterModule(rtp_rtcp.get());
}

void PayloadRouter::SetActive(bool active) {
  rtc::CritScope lock(&crit_);
  if (active_ == active)
    return;
  const std::vector<bool> active_modules(rtp_modules_.size(), active);
  SetActiveModules(active_modules);
}

void PayloadRouter::SetActiveModules(const std::vector<bool> active_modules) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK_EQ(rtp_modules_.size(), active_modules.size());
  active_ = false;
  for (size_t i = 0; i < active_modules.size(); ++i) {
    if (active_modules[i]) {
      active_ = true;
    }
    // Sends a kRtcpByeCode when going from true to false.
    rtp_modules_[i]->SetSendingStatus(active_modules[i]);
    // If set to false this module won't send media.
    rtp_modules_[i]->SetSendingMediaStatus(active_modules[i]);
  }
}

bool PayloadRouter::IsActive() {
  rtc::CritScope lock(&crit_);
  return active_ && !rtp_modules_.empty();
}

EncodedImageCallback::Result PayloadRouter::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(!rtp_modules_.empty());
  if (!active_)
    return Result(Result::ERROR_SEND_FAILED);

  RTPVideoHeader rtp_video_header;
  if (codec_specific_info)
    CopyCodecSpecific(codec_specific_info, &rtp_video_header);

  rtp_video_header.rotation = encoded_image.rotation_;
  rtp_video_header.content_type = encoded_image.content_type_;
  rtp_video_header.playout_delay = encoded_image.playout_delay_;

  SetVideoTiming(&rtp_video_header.video_timing, encoded_image);

  int stream_index = rtp_video_header.simulcastIdx;
  RTC_DCHECK_LT(stream_index, rtp_modules_.size());

  // Sets picture id and tl0 pic idx.
  const bool first_frame_in_picture =
      (codec_specific_info && codec_specific_info->codecType == kVideoCodecVP9)
          ? codec_specific_info->codecSpecific.VP9.first_frame_in_picture
          : true;
  params_[stream_index].Set(&rtp_video_header, first_frame_in_picture);

  uint32_t frame_id;
  if (!rtp_modules_[stream_index]->Sending()) {
    // The payload router could be active but this module isn't sending.
    return Result(Result::ERROR_SEND_FAILED);
  }
  bool send_result = rtp_modules_[stream_index]->SendOutgoingData(
      encoded_image._frameType, rtp_config_.payload_type,
      encoded_image._timeStamp, encoded_image.capture_time_ms_,
      encoded_image._buffer, encoded_image._length, fragmentation,
      &rtp_video_header, &frame_id);
  if (!send_result)
    return Result(Result::ERROR_SEND_FAILED);

  return Result(Result::OK, frame_id);
}

void PayloadRouter::OnBitrateAllocationUpdated(
    const VideoBitrateAllocation& bitrate) {
  rtc::CritScope lock(&crit_);
  if (IsActive()) {
    if (rtp_modules_.size() == 1) {
      // If spatial scalability is enabled, it is covered by a single stream.
      rtp_modules_[0]->SetVideoBitrateAllocation(bitrate);
    } else {
      // Simulcast is in use, split the VideoBitrateAllocation into one struct
      // per rtp stream, moving over the temporal layer allocation.
      for (size_t si = 0; si < rtp_modules_.size(); ++si) {
        // Don't send empty TargetBitrate messages on streams not being relayed.
        if (!bitrate.IsSpatialLayerUsed(si)) {
          // The next spatial layer could be used if the current one is
          // inactive.
          continue;
        }

        VideoBitrateAllocation layer_bitrate;
        for (int tl = 0; tl < kMaxTemporalStreams; ++tl) {
          if (bitrate.HasBitrate(si, tl))
            layer_bitrate.SetBitrate(0, tl, bitrate.GetBitrate(si, tl));
        }
        rtp_modules_[si]->SetVideoBitrateAllocation(layer_bitrate);
      }
    }
  }
}

void PayloadRouter::ConfigureProtection(const Rtp& rtp_config) {
  //  RTC_DCHECK_RUN_ON(worker_queue_);

  // Consistency of FlexFEC parameters is checked in MaybeCreateFlexfecSender.
  const bool flexfec_enabled = (flexfec_sender_ != nullptr);

  // Consistency of NACK and RED+ULPFEC parameters is checked in this function.
  const bool nack_enabled = rtp_config.nack.rtp_history_ms > 0;
  int red_payload_type = rtp_config.ulpfec.red_payload_type;
  int ulpfec_payload_type = rtp_config.ulpfec.ulpfec_payload_type;

  // Shorthands.
  auto IsRedEnabled = [&]() { return red_payload_type >= 0; };
  auto DisableRed = [&]() { red_payload_type = -1; };
  auto IsUlpfecEnabled = [&]() { return ulpfec_payload_type >= 0; };
  auto DisableUlpfec = [&]() { ulpfec_payload_type = -1; };

  if (webrtc::field_trial::IsEnabled("WebRTC-DisableUlpFecExperiment")) {
    RTC_LOG(LS_INFO) << "Experiment to disable sending ULPFEC is enabled.";
    DisableUlpfec();
  }

  // If enabled, FlexFEC takes priority over RED+ULPFEC.
  if (flexfec_enabled) {
    // We can safely disable RED here, because if the remote supports FlexFEC,
    // we know that it has a receiver without the RED/RTX workaround.
    // See http://crbug.com/webrtc/6650 for more information.
    if (IsRedEnabled()) {
      RTC_LOG(LS_INFO) << "Both FlexFEC and RED are configured. Disabling RED.";
      DisableRed();
    }
    if (IsUlpfecEnabled()) {
      RTC_LOG(LS_INFO)
          << "Both FlexFEC and ULPFEC are configured. Disabling ULPFEC.";
      DisableUlpfec();
    }
  }

  // Payload types without picture ID cannot determine that a stream is complete
  // without retransmitting FEC, so using ULPFEC + NACK for H.264 (for instance)
  // is a waste of bandwidth since FEC packets still have to be transmitted.
  // Note that this is not the case with FlexFEC.
  if (nack_enabled && IsUlpfecEnabled() &&
      !PayloadTypeSupportsSkippingFecPackets(rtp_config.payload_name)) {
    RTC_LOG(LS_WARNING)
        << "Transmitting payload type without picture ID using "
           "NACK+ULPFEC is a waste of bandwidth since ULPFEC packets "
           "also have to be retransmitted. Disabling ULPFEC.";
    DisableUlpfec();
  }

  // Verify payload types.
  //
  // Due to how old receivers work, we need to always send RED if it has been
  // negotiated. This is a remnant of an old RED/RTX workaround, see
  // https://codereview.webrtc.org/2469093003.
  // TODO(brandtr): This change went into M56, so we can remove it in ~M59.
  // At that time, we can disable RED whenever ULPFEC is disabled, as there is
  // no point in using RED without ULPFEC.
  if (IsRedEnabled()) {
    RTC_DCHECK_GE(red_payload_type, 0);
    RTC_DCHECK_LE(red_payload_type, 127);
  }
  if (IsUlpfecEnabled()) {
    RTC_DCHECK_GE(ulpfec_payload_type, 0);
    RTC_DCHECK_LE(ulpfec_payload_type, 127);
    if (!IsRedEnabled()) {
      RTC_LOG(LS_WARNING)
          << "ULPFEC is enabled but RED is disabled. Disabling ULPFEC.";
      DisableUlpfec();
    }
  }

  for (auto& rtp_rtcp : rtp_modules_) {
    // Set NACK.
    rtp_rtcp->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);
    // Set RED/ULPFEC information.
    rtp_rtcp->SetUlpfecConfig(red_payload_type, ulpfec_payload_type);
  }
}

bool PayloadRouter::FecEnabled() const {
  const bool flexfec_enabled = (flexfec_sender_ != nullptr);
  int ulpfec_payload_type = rtp_config_.ulpfec.ulpfec_payload_type;
  return flexfec_enabled || ulpfec_payload_type >= 0;
}

bool PayloadRouter::NackEnabled() const {
  const bool nack_enabled = rtp_config_.nack.rtp_history_ms > 0;
  return nack_enabled;
}

void PayloadRouter::DeliverRtcp(const uint8_t* packet, size_t length) {
  // Runs on a network thread.
  // RTC_DCHECK(!worker_queue_->IsCurrent());
  for (auto& rtp_rtcp : rtp_modules_)
    rtp_rtcp->IncomingRtcpPacket(packet, length);
}

void PayloadRouter::ProtectionRequest(const FecProtectionParams* delta_params,
                                      const FecProtectionParams* key_params,
                                      uint32_t* sent_video_rate_bps,
                                      uint32_t* sent_nack_rate_bps,
                                      uint32_t* sent_fec_rate_bps) {
  *sent_video_rate_bps = 0;
  *sent_nack_rate_bps = 0;
  *sent_fec_rate_bps = 0;
  for (auto& rtp_rtcp : rtp_modules_) {
    uint32_t not_used = 0;
    uint32_t module_video_rate = 0;
    uint32_t module_fec_rate = 0;
    uint32_t module_nack_rate = 0;
    rtp_rtcp->SetFecParameters(*delta_params, *key_params);
    rtp_rtcp->BitrateSent(&not_used, &module_video_rate, &module_fec_rate,
                          &module_nack_rate);
    *sent_video_rate_bps += module_video_rate;
    *sent_nack_rate_bps += module_nack_rate;
    *sent_fec_rate_bps += module_fec_rate;
  }
}

void PayloadRouter::SetMaxRtpPacketSize(size_t max_rtp_packet_size) {
  for (auto& rtp_rtcp : rtp_modules_) {
    rtp_rtcp->SetMaxRtpPacketSize(max_rtp_packet_size);
  }
}

void PayloadRouter::ConfigureSsrcs(const Rtp& rtp_config) {
  // RTC_DCHECK_RUN_ON(worker_queue_);
  // Configure regular SSRCs.
  for (size_t i = 0; i < rtp_config.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_modules_[i].get();
    rtp_rtcp->SetSSRC(ssrc);

    // Restore RTP state if previous existed.
    auto it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtpState(it->second);
  }

  // Set up RTX if available.
  if (rtp_config.rtx.ssrcs.empty())
    return;

  // Configure RTX SSRCs.
  RTC_DCHECK_EQ(rtp_config.rtx.ssrcs.size(), rtp_config.ssrcs.size());
  for (size_t i = 0; i < rtp_config.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config.rtx.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_modules_[i].get();
    rtp_rtcp->SetRtxSsrc(ssrc);
    auto it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtxState(it->second);
  }

  // Configure RTX payload types.
  RTC_DCHECK_GE(rtp_config.rtx.payload_type, 0);
  for (auto& rtp_rtcp : rtp_modules_) {
    rtp_rtcp->SetRtxSendPayloadType(rtp_config.rtx.payload_type,
                                    rtp_config.payload_type);
    rtp_rtcp->SetRtxSendStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  }
  if (rtp_config.ulpfec.red_payload_type != -1 &&
      rtp_config.ulpfec.red_rtx_payload_type != -1) {
    for (auto& rtp_rtcp : rtp_modules_) {
      rtp_rtcp->SetRtxSendPayloadType(rtp_config.ulpfec.red_rtx_payload_type,
                                      rtp_config.ulpfec.red_payload_type);
    }
  }
}

void PayloadRouter::OnNetworkAvailability(bool network_available) {
  for (auto& rtp_rtcp : rtp_modules_) {
    rtp_rtcp->SetRTCPStatus(network_available ? rtp_config_.rtcp_mode
                                              : RtcpMode::kOff);
  }
}

std::map<uint32_t, RtpState> PayloadRouter::GetRtpStates() const {
  // RTC_DCHECK_RUN_ON(worker_queue_);
  std::map<uint32_t, RtpState> rtp_states;

  for (size_t i = 0; i < rtp_config_.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.ssrcs[i];
    RTC_DCHECK_EQ(ssrc, rtp_modules_[i]->SSRC());
    rtp_states[ssrc] = rtp_modules_[i]->GetRtpState();
  }

  for (size_t i = 0; i < rtp_config_.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.rtx.ssrcs[i];
    rtp_states[ssrc] = rtp_modules_[i]->GetRtxState();
  }

  if (flexfec_sender_) {
    uint32_t ssrc = rtp_config_.flexfec.ssrc;
    rtp_states[ssrc] = flexfec_sender_->GetRtpState();
  }

  return rtp_states;
}

std::map<uint32_t, RtpPayloadState> PayloadRouter::GetRtpPayloadStates() const {
  rtc::CritScope lock(&crit_);
  std::map<uint32_t, RtpPayloadState> payload_states;
  for (const auto& param : params_) {
    payload_states[param.ssrc()] = param.state();
  }
  return payload_states;
}
}  // namespace webrtc
