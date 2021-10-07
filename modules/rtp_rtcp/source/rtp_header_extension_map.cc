/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"

#include "absl/strings/string_view.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

template <typename Extension>
constexpr absl::string_view CreateExtensionInfo() {
  return Extension::Uri();
}

constexpr absl::string_view kExtensions[] = {
    CreateExtensionInfo<TransmissionOffset>(),
    CreateExtensionInfo<AudioLevel>(),
    CreateExtensionInfo<CsrcAudioLevel>(),
    CreateExtensionInfo<AbsoluteSendTime>(),
    CreateExtensionInfo<AbsoluteCaptureTimeExtension>(),
    CreateExtensionInfo<VideoOrientation>(),
    CreateExtensionInfo<TransportSequenceNumber>(),
    CreateExtensionInfo<TransportSequenceNumberV2>(),
    CreateExtensionInfo<PlayoutDelayLimits>(),
    CreateExtensionInfo<VideoContentTypeExtension>(),
    CreateExtensionInfo<RtpVideoLayersAllocationExtension>(),
    CreateExtensionInfo<VideoTimingExtension>(),
    CreateExtensionInfo<RtpStreamId>(),
    CreateExtensionInfo<RepairedRtpStreamId>(),
    CreateExtensionInfo<RtpMid>(),
    CreateExtensionInfo<RtpGenericFrameDescriptorExtension00>(),
    CreateExtensionInfo<RtpDependencyDescriptorExtension>(),
    CreateExtensionInfo<ColorSpaceExtension>(),
    CreateExtensionInfo<InbandComfortNoiseExtension>(),
    CreateExtensionInfo<VideoFrameTrackingIdExtension>(),
};

}  // namespace

constexpr int RtpHeaderExtensionMap::kInvalidId;
constexpr absl::string_view RtpHeaderExtensionMap::kInvalidUri;

rtc::ArrayView<const absl::string_view>
RtpHeaderExtensionMap::KnownExtensions() {
  return kExtensions;
}

RtpHeaderExtensionMap::RtpHeaderExtensionMap() : RtpHeaderExtensionMap(false) {}

RtpHeaderExtensionMap::RtpHeaderExtensionMap(bool extmap_allow_mixed)
    : extmap_allow_mixed_(extmap_allow_mixed) {}

RtpHeaderExtensionMap::RtpHeaderExtensionMap(
    rtc::ArrayView<const RtpExtension> extensions)
    : RtpHeaderExtensionMap(false) {
  for (const RtpExtension& extension : extensions)
    RegisterByUri(extension.id, extension.uri);
}

void RtpHeaderExtensionMap::Reset(
    rtc::ArrayView<const RtpExtension> extensions) {
  mapping_ = {};
  for (const RtpExtension& extension : extensions)
    RegisterByUri(extension.id, extension.uri);
}

bool RtpHeaderExtensionMap::RegisterByUri(int id, absl::string_view uri) {
  for (absl::string_view extension : kExtensions)
    if (uri == extension)
      return UnsafeRegisterByUri(id, extension);
  RTC_LOG(LS_WARNING) << "Unknown extension uri:'" << uri << "', id: " << id
                      << '.';
  return false;
}

void RtpHeaderExtensionMap::Deregister(absl::string_view uri) {
  for (auto it = mapping_.begin(); it != mapping_.end(); ++it) {
    if (it->uri == uri) {
      mapping_.erase(it);
      return;
    }
  }
}

bool RtpHeaderExtensionMap::UnsafeRegisterByUri(int id, absl::string_view uri) {
  if (id < RtpExtension::kMinId || id > RtpExtension::kMaxId) {
    RTC_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                        << "' with invalid id:" << id << ".";
    return false;
  }

  for (const auto& entry : mapping_) {
    if (entry.uri == uri) {
      RTC_DCHECK_EQ(entry.uri.data(), uri.data());
      if (entry.id == id) {
        // Already registered with the same id.
        return true;
      }
      RTC_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                          << "', id:" << id
                          << ". Uri already in use by extension id "
                          << entry.id;
      return false;
    }

    if (entry.id == id) {
      RTC_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                          << "', id:" << id
                          << ". Id already in use by extension " << entry.uri;
      return false;
    }
  }

  Entry new_entry;
  new_entry.id = id;
  new_entry.uri = uri;
  mapping_.push_back(new_entry);
  return true;
}

int RtpHeaderExtensionMap::UnsafeId(absl::string_view uri) const {
  for (const auto& entry : mapping_) {
    if (entry.uri.data() == uri.data()) {
      RTC_DCHECK_EQ(entry.uri.size(), uri.size());
      return entry.id;
    }
    RTC_DCHECK_NE(entry.uri, uri);
  }
  return kInvalidId;
}

int RtpHeaderExtensionMap::Id(absl::string_view uri) const {
  for (const auto& entry : mapping_) {
    if (entry.uri == uri) {
      return entry.id;
    }
  }
  return kInvalidId;
}

absl::string_view RtpHeaderExtensionMap::Uri(int id) const {
  for (const auto& entry : mapping_) {
    if (entry.id == id) {
      return entry.uri;
    }
  }
  return kInvalidUri;
}

}  // namespace webrtc
