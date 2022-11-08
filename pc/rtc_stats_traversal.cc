/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtc_stats_traversal.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/stats/rtcstats_objects.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

void TraverseAndTakeVisitedStats(RTCStatsReport* report,
                                 RTCStatsReport* visited_report,
                                 const std::string& current_id) {
  // Mark current stats object as visited by moving it `report` to
  // `visited_report`.
  std::unique_ptr<const RTCStats> current = report->Take(current_id);
  if (!current) {
    // This node has already been visited (or it is an invalid id).
    return;
  }
  std::vector<const std::string*> neighbor_ids =
      GetStatsReferencedIds(*current);
  visited_report->AddStats(std::move(current));

  // Recursively traverse all neighbors.
  for (const auto* neighbor_id : neighbor_ids) {
    TraverseAndTakeVisitedStats(report, visited_report, *neighbor_id);
  }
}

void AddIdIfDefined(const RTCStatsMember<std::string>& id,
                    std::vector<const std::string*>* neighbor_ids) {
  if (id.is_defined())
    neighbor_ids->push_back(&(*id));
}

void AddIdsIfDefined(const RTCStatsMember<std::vector<std::string>>& ids,
                     std::vector<const std::string*>* neighbor_ids) {
  if (ids.is_defined()) {
    for (const std::string& id : *ids)
      neighbor_ids->push_back(&id);
  }
}

}  // namespace

rtc::scoped_refptr<RTCStatsReport> TakeReferencedStats(
    rtc::scoped_refptr<RTCStatsReport> report,
    const std::vector<std::string>& ids) {
  rtc::scoped_refptr<RTCStatsReport> result =
      RTCStatsReport::Create(report->timestamp());
  for (const auto& id : ids) {
    TraverseAndTakeVisitedStats(report.get(), result.get(), id);
  }
  return result;
}

std::vector<const std::string*> GetStatsReferencedIds(const RTCStats& stats) {
  std::vector<const std::string*> neighbor_ids;
  RTCStatsType stats_type = stats.StatsType();
  if (stats_type == RTCCertificateStats::kStatsType) {
    const auto& certificate = static_cast<const RTCCertificateStats&>(stats);
    AddIdIfDefined(certificate.issuer_certificate_id, &neighbor_ids);
  } else if (stats_type == RTCCodecStats::kStatsType) {
    const auto& codec = static_cast<const RTCCodecStats&>(stats);
    AddIdIfDefined(codec.transport_id, &neighbor_ids);
  } else if (stats_type == RTCDataChannelStats::kStatsType) {
    // RTCDataChannelStats does not have any neighbor references.
  } else if (stats_type == RTCIceCandidatePairStats::kStatsType) {
    const auto& candidate_pair =
        static_cast<const RTCIceCandidatePairStats&>(stats);
    AddIdIfDefined(candidate_pair.transport_id, &neighbor_ids);
    AddIdIfDefined(candidate_pair.local_candidate_id, &neighbor_ids);
    AddIdIfDefined(candidate_pair.remote_candidate_id, &neighbor_ids);
  } else if (stats_type == RTCLocalIceCandidateStats::kStatsType ||
             stats_type == RTCRemoteIceCandidateStats::kStatsType) {
    const auto& local_or_remote_candidate =
        static_cast<const RTCIceCandidateStats&>(stats);
    AddIdIfDefined(local_or_remote_candidate.transport_id, &neighbor_ids);
  } else if (stats_type == DEPRECATED_RTCMediaStreamStats::kStatsType) {
    const auto& stream =
        static_cast<const DEPRECATED_RTCMediaStreamStats&>(stats);
    AddIdsIfDefined(stream.track_ids, &neighbor_ids);
  } else if (stats_type == DEPRECATED_RTCMediaStreamTrackStats::kStatsType) {
    const auto& track =
        static_cast<const DEPRECATED_RTCMediaStreamTrackStats&>(stats);
    AddIdIfDefined(track.media_source_id, &neighbor_ids);
  } else if (stats_type == RTCPeerConnectionStats::kStatsType) {
    // RTCPeerConnectionStats does not have any neighbor references.
  } else if (stats_type == RTCInboundRTPStreamStats::kStatsType) {
    const auto& inbound_rtp =
        static_cast<const RTCInboundRTPStreamStats&>(stats);
    AddIdIfDefined(inbound_rtp.remote_id, &neighbor_ids);
    AddIdIfDefined(inbound_rtp.track_id, &neighbor_ids);
    AddIdIfDefined(inbound_rtp.transport_id, &neighbor_ids);
    AddIdIfDefined(inbound_rtp.codec_id, &neighbor_ids);
  } else if (stats_type == RTCOutboundRTPStreamStats::kStatsType) {
    const auto& outbound_rtp =
        static_cast<const RTCOutboundRTPStreamStats&>(stats);
    AddIdIfDefined(outbound_rtp.remote_id, &neighbor_ids);
    AddIdIfDefined(outbound_rtp.track_id, &neighbor_ids);
    AddIdIfDefined(outbound_rtp.transport_id, &neighbor_ids);
    AddIdIfDefined(outbound_rtp.codec_id, &neighbor_ids);
    AddIdIfDefined(outbound_rtp.media_source_id, &neighbor_ids);
  } else if (stats_type == RTCRemoteInboundRtpStreamStats::kStatsType) {
    const auto& remote_inbound_rtp =
        static_cast<const RTCRemoteInboundRtpStreamStats&>(stats);
    AddIdIfDefined(remote_inbound_rtp.transport_id, &neighbor_ids);
    AddIdIfDefined(remote_inbound_rtp.codec_id, &neighbor_ids);
    AddIdIfDefined(remote_inbound_rtp.local_id, &neighbor_ids);
  } else if (stats_type == RTCRemoteOutboundRtpStreamStats::kStatsType) {
    const auto& remote_outbound_rtp =
        static_cast<const RTCRemoteOutboundRtpStreamStats&>(stats);
    // Inherited from `RTCRTPStreamStats`.
    AddIdIfDefined(remote_outbound_rtp.track_id, &neighbor_ids);
    AddIdIfDefined(remote_outbound_rtp.transport_id, &neighbor_ids);
    AddIdIfDefined(remote_outbound_rtp.codec_id, &neighbor_ids);
    // Direct members of `RTCRemoteOutboundRtpStreamStats`.
    AddIdIfDefined(remote_outbound_rtp.local_id, &neighbor_ids);
  } else if (stats_type == RTCAudioSourceStats::kStatsType ||
             stats_type == RTCVideoSourceStats::kStatsType) {
    // RTC[Audio/Video]SourceStats does not have any neighbor references.
  } else if (stats_type == RTCTransportStats::kStatsType) {
    // RTCTransportStats does not have any neighbor references.
    const auto& transport = static_cast<const RTCTransportStats&>(stats);
    AddIdIfDefined(transport.rtcp_transport_stats_id, &neighbor_ids);
    AddIdIfDefined(transport.selected_candidate_pair_id, &neighbor_ids);
    AddIdIfDefined(transport.local_certificate_id, &neighbor_ids);
    AddIdIfDefined(transport.remote_certificate_id, &neighbor_ids);
  } else {
    RTC_DCHECK_NOTREACHED()
        << "Unrecognized type: " << static_cast<int>(stats_type);
  }
  return neighbor_ids;
}

}  // namespace webrtc
