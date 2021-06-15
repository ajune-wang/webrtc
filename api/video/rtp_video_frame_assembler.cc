/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/rtp_video_frame_assembler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "api/video/encoded_frame.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_av1.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_generic.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_raw.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp9.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpVideoFrameAssembler::RtpVideoFrameAssembler(PayloadFormat payload_format)
    : packet_buffer_(512, 2048) {
  switch (payload_format) {
    case kRaw: {
      depacketizer_.emplace<VideoRtpDepacketizerRaw>();
      break;
    }
    case kH264: {
      depacketizer_.emplace<VideoRtpDepacketizerH264>();
      break;
    }
    case kVp8: {
      depacketizer_.emplace<VideoRtpDepacketizerVp8>();
      break;
    }
    case kVp9: {
      depacketizer_.emplace<VideoRtpDepacketizerVp9>();
      break;
    }
    case kAv1: {
      depacketizer_.emplace<VideoRtpDepacketizerAv1>();
      break;
    }
    case kGenericPayload: {
      depacketizer_.emplace<VideoRtpDepacketizerGeneric>();
      break;
    }
  }
}

std::vector<std::unique_ptr<RtpFrameObject>>
RtpVideoFrameAssembler::InsertPacket(const RtpPacketReceived& rtp_packet) {
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload =
      GetDepacketizer().Parse(rtp_packet.PayloadBuffer());

  if (parsed_payload == absl::nullopt) {
    return {};
  }

  if (parsed_payload->video_payload.size() == 0) {
    auto frames = UpdateWithPadding(rtp_packet.SequenceNumber());
    SaveFrameIdToSeqNumMapping(frames);
    return frames;
  }

  if (rtp_packet.HasExtension<RtpDependencyDescriptorExtension>()) {
    if (!ParseDependenciesDescriptorExtension(rtp_packet,
                                              parsed_payload->video_header)) {
      return {};
    }
  } else if (rtp_packet.HasExtension<RtpGenericFrameDescriptorExtension00>()) {
    if (!ParseGenericDescriptorExtension(rtp_packet,
                                         parsed_payload->video_header)) {
      return {};
    }
  }

  parsed_payload->video_header.is_last_packet_in_frame |= rtp_packet.Marker();

  auto packet = std::make_unique<video_coding::PacketBuffer::Packet>(
      rtp_packet, parsed_payload->video_header);
  packet->video_payload = std::move(parsed_payload->video_payload);

  auto frames = FindReferences(
      AssembleFrames(packet_buffer_.InsertPacket(std::move(packet))));
  SaveFrameIdToSeqNumMapping(frames);
  return frames;
}

void RtpVideoFrameAssembler::ClearTo(int64_t frame_id) {
  auto it = frame_id_to_seq_num_.find(frame_id);
  if (it != frame_id_to_seq_num_.end()) {
    packet_buffer_.ClearTo(it->second);
    reference_finder_.ClearTo(it->second);
    frame_id_to_seq_num_.erase(frame_id_to_seq_num_.begin(), it);
  }
}

std::vector<std::unique_ptr<RtpFrameObject>>
RtpVideoFrameAssembler::AssembleFrames(
    video_coding::PacketBuffer::InsertResult insert_result) {
  video_coding::PacketBuffer::Packet* first_packet = nullptr;
  std::vector<rtc::ArrayView<const uint8_t>> payloads;
  std::vector<std::unique_ptr<RtpFrameObject>> result;

  for (auto& packet : insert_result.packets) {
    if (packet->is_first_packet_in_frame()) {
      first_packet = packet.get();
      payloads.clear();
    }
    payloads.emplace_back(packet->video_payload);

    if (packet->is_last_packet_in_frame()) {
      rtc::scoped_refptr<EncodedImageBuffer> bitstream =
          GetDepacketizer().AssembleFrame(payloads);

      if (!bitstream) {
        continue;
      }

      const video_coding::PacketBuffer::Packet& last_packet = *packet;
      result.push_back(std::make_unique<RtpFrameObject>(
          first_packet->seq_num,                  //
          last_packet.seq_num,                    //
          last_packet.marker_bit,                 //
          /*times_nacked=*/0,                     //
          /*first_packet_received_time=*/0,       //
          /*last_packet_received_time=*/0,        //
          first_packet->timestamp,                //
          /*ntp_time_ms=*/0,                      //
          /*timing=*/VideoSendTiming(),           //
          first_packet->payload_type,             //
          first_packet->codec(),                  //
          last_packet.video_header.rotation,      //
          last_packet.video_header.content_type,  //
          first_packet->video_header,             //
          last_packet.video_header.color_space,   //
          /*packet_infos=*/RtpPacketInfos(),      //
          std::move(bitstream)));
    }
  }

  return result;
}

std::vector<std::unique_ptr<RtpFrameObject>>
RtpVideoFrameAssembler::FindReferences(
    std::vector<std::unique_ptr<RtpFrameObject>> frames) {
  std::vector<std::unique_ptr<RtpFrameObject>> res;
  for (auto& frame : frames) {
    auto complete_frames = reference_finder_.ManageFrame(std::move(frame));
    for (std::unique_ptr<RtpFrameObject>& complete_frame : complete_frames) {
      res.push_back(std::move(complete_frame));
    }
  }
  return res;
}

std::vector<std::unique_ptr<RtpFrameObject>>
RtpVideoFrameAssembler::UpdateWithPadding(uint16_t seq_num) {
  auto res =
      FindReferences(AssembleFrames(packet_buffer_.InsertPadding(seq_num)));

  auto ref_finder_update = reference_finder_.PaddingReceived(seq_num);

  res.insert(res.end(), std::make_move_iterator(ref_finder_update.begin()),
             std::make_move_iterator(ref_finder_update.end()));

  return res;
}

void RtpVideoFrameAssembler::SaveFrameIdToSeqNumMapping(
    const std::vector<std::unique_ptr<RtpFrameObject>>& frames) {
  for (auto& frame : frames) {
    frame_id_to_seq_num_[frame->Id()] = frame->last_seq_num();
  }
}

bool RtpVideoFrameAssembler::ParseDependenciesDescriptorExtension(
    const RtpPacketReceived& rtp_packet,
    RTPVideoHeader& video_header) {
  webrtc::DependencyDescriptor dependency_descriptor;

  if (!rtp_packet.GetExtension<RtpDependencyDescriptorExtension>(
          video_structure_.get(), &dependency_descriptor)) {
    // Descriptor is either malformed, or the template referenced is not in
    // `video_structure_` that is currently being held.
    // TODO(bugs.webrtc.org/10342): Improve packet reordering behavior.
    RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                        << " Failed to parse dependency descriptor.";
    return false;
  }

  if (dependency_descriptor.attached_structure != nullptr &&
      !dependency_descriptor.first_packet_in_frame) {
    RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                        << "Invalid dependency descriptor: structure "
                           "attached to non first packet of a frame.";
    return false;
  }

  video_header.is_first_packet_in_frame =
      dependency_descriptor.first_packet_in_frame;
  video_header.is_last_packet_in_frame =
      dependency_descriptor.last_packet_in_frame;

  int64_t frame_id =
      frame_id_unwrapper_.Unwrap(dependency_descriptor.frame_number);
  auto& generic_descriptor_info = video_header.generic.emplace();
  generic_descriptor_info.frame_id = frame_id;
  generic_descriptor_info.spatial_index =
      dependency_descriptor.frame_dependencies.spatial_id;
  generic_descriptor_info.temporal_index =
      dependency_descriptor.frame_dependencies.temporal_id;
  for (int fdiff : dependency_descriptor.frame_dependencies.frame_diffs) {
    generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
  }
  generic_descriptor_info.decode_target_indications =
      dependency_descriptor.frame_dependencies.decode_target_indications;
  if (dependency_descriptor.resolution) {
    video_header.width = dependency_descriptor.resolution->Width();
    video_header.height = dependency_descriptor.resolution->Height();
  }

  // FrameDependencyStructure is sent in the dependency descriptor of the first
  // packet of a key frame and is required to parse all subsequent packets until
  // the next key frame.
  if (dependency_descriptor.attached_structure) {
    RTC_DCHECK(dependency_descriptor.first_packet_in_frame);
    if (video_structure_frame_id_ > frame_id) {
      RTC_LOG(LS_WARNING)
          << "Arrived key frame with id " << frame_id << " and structure id "
          << dependency_descriptor.attached_structure->structure_id
          << " is older than the latest received key frame with id "
          << *video_structure_frame_id_ << " and structure id "
          << video_structure_->structure_id;
      return false;
    }
    video_structure_ = std::move(dependency_descriptor.attached_structure);
    video_structure_frame_id_ = frame_id;
    video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  }
  return true;
}

bool RtpVideoFrameAssembler::ParseGenericDescriptorExtension(
    const RtpPacketReceived& rtp_packet,
    RTPVideoHeader& video_header) {
  RtpGenericFrameDescriptor generic_frame_descriptor;
  if (!rtp_packet.GetExtension<RtpGenericFrameDescriptorExtension00>(
          &generic_frame_descriptor)) {
    return false;
  }

  video_header.is_first_packet_in_frame =
      generic_frame_descriptor.FirstPacketInSubFrame();
  video_header.is_last_packet_in_frame =
      generic_frame_descriptor.LastPacketInSubFrame();

  if (generic_frame_descriptor.FirstPacketInSubFrame()) {
    video_header.frame_type =
        generic_frame_descriptor.FrameDependenciesDiffs().empty()
            ? VideoFrameType::kVideoFrameKey
            : VideoFrameType::kVideoFrameDelta;

    auto& generic_descriptor_info = video_header.generic.emplace();
    int64_t frame_id =
        frame_id_unwrapper_.Unwrap(generic_frame_descriptor.FrameId());
    generic_descriptor_info.frame_id = frame_id;
    generic_descriptor_info.spatial_index =
        generic_frame_descriptor.SpatialLayer();
    generic_descriptor_info.temporal_index =
        generic_frame_descriptor.TemporalLayer();
    for (uint16_t fdiff : generic_frame_descriptor.FrameDependenciesDiffs()) {
      generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
    }
  }
  video_header.width = generic_frame_descriptor.Width();
  video_header.height = generic_frame_descriptor.Height();
  return true;
}

VideoRtpDepacketizer& RtpVideoFrameAssembler::GetDepacketizer() {
  return absl::visit([](auto& impl) -> VideoRtpDepacketizer& { return impl; },
                     depacketizer_);
}

}  // namespace webrtc
