/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SDP_OFFER_ANSWER_PLAN_B_H_
#define PC_SDP_OFFER_ANSWER_PLAN_B_H_

#include "pc/sdp_offer_answer.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/media_stream_proxy.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/uma_metrics.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "media/base/codec.h"
#include "media/base/media_engine.h"
#include "media/base/rid_description.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_description_factory.h"
#include "p2p/base/transport_info.h"
#include "pc/connection_context.h"
#include "pc/data_channel_utils.h"
#include "pc/media_protocol_names.h"
#include "pc/media_stream.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_message_handler.h"
#include "pc/rtp_data_channel.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_transport.h"
#include "pc/sdp_offer_answer_unified_plan.h"
#include "pc/simulcast_description.h"
#include "pc/stats_collector.h"
#include "pc/usage_pattern.h"
#include "pc/webrtc_session_description_factory.h"
#include "rtc_base/bind.h"
#include "rtc_base/helpers.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::MediaProtocolType;
using cricket::RidDescription;
using cricket::RidDirection;
using cricket::SessionDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;
using cricket::StreamParams;
using cricket::TransportInfo;

using cricket::LOCAL_PORT_TYPE;
using cricket::PRFLX_PORT_TYPE;
using cricket::RELAY_PORT_TYPE;
using cricket::STUN_PORT_TYPE;

namespace webrtc {

class SdpOfferAnswerHandlerPlanB : public SdpOfferAnswerHandler {
 public:
  explicit SdpOfferAnswerHandlerPlanB(PeerConnection* pc)
      : SdpOfferAnswerHandler(pc) {}

 private:
  void OnOperationsChainEmpty() override;
  bool ShouldFireNegotiationNeededEvent(uint32_t event_id) override;

  void UpdateNegotiationNeeded() override;

  RTCError ApplyLocalDescriptionByPlan(
      SdpType type,
      const SessionDescriptionInterface* old_local_description) override;
  void SetLocalRollbackCompleteByPlan(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer,
      const SessionDescriptionInterface* desc) override;
  bool SetRemoteRollbackCompleteByPlan(
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer,
      const SessionDescriptionInterface* desc) override;

  RTCError ApplyRemoteDescriptionByPlan(SdpType type) override;
  RTCError UpdateChannelsByPlan(
      SdpType type,
      const SessionDescriptionInterface* old_remote_description) override;
};

}  // namespace webrtc

#endif  // PC_SDP_OFFER_ANSWER_PLAN_B_H_
