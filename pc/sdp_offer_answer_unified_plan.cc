/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_offer_answer_unified_plan.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/dtls_transport_interface.h"
#include "api/media_stream_interface.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "pc/channel.h"
#include "pc/data_channel_controller.h"
#include "pc/data_channel_utils.h"
#include "pc/jsep_transport_controller.h"
#include "pc/media_session.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transmission_manager.h"
#include "pc/stats_collector.h"
#include "pc/stream_collection.h"
#include "pc/transceiver_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/operations_chain.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/unique_id_generator.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// UMA metric names
const char kSimulcastDisabled[] = "WebRTC.PeerConnection.Simulcast.Disabled";

// This method will extract any send encodings that were sent by the remote
// connection. This is currently only relevant for Simulcast scenario (where
// the number of layers may be communicated by the server).
std::vector<RtpEncodingParameters> GetSendEncodingsFromRemoteDescription(
    const MediaContentDescription& desc) {
  if (!desc.HasSimulcast()) {
    return {};
  }
  std::vector<RtpEncodingParameters> result;
  const SimulcastDescription& simulcast = desc.simulcast_description();

  // This is a remote description, the parameters we are after should appear
  // as receive streams.
  for (const auto& alternatives : simulcast.receive_layers()) {
    RTC_DCHECK(!alternatives.empty());
    // There is currently no way to specify or choose from alternatives.
    // We will always use the first alternative, which is the most preferred.
    const SimulcastLayer& layer = alternatives[0];
    RtpEncodingParameters parameters;
    parameters.rid = layer.rid;
    parameters.active = !layer.is_paused;
    result.push_back(parameters);
  }

  return result;
}

RTCError UpdateSimulcastLayerStatusInSender(
    const std::vector<SimulcastLayer>& layers,
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternal();
  std::vector<std::string> disabled_layers;

  // The simulcast envelope cannot be changed, only the status of the streams.
  // So we will iterate over the send encodings rather than the layers.
  for (RtpEncodingParameters& encoding : parameters.encodings) {
    auto iter = std::find_if(layers.begin(), layers.end(),
                             [&encoding](const SimulcastLayer& layer) {
                               return layer.rid == encoding.rid;
                             });
    // A layer that cannot be found may have been removed by the remote party.
    if (iter == layers.end()) {
      disabled_layers.push_back(encoding.rid);
      continue;
    }

    encoding.active = !iter->is_paused;
  }

  RTCError result = sender->SetParametersInternal(parameters);
  if (result.ok()) {
    result = sender->DisableEncodingLayers(disabled_layers);
  }

  return result;
}

bool SimulcastIsRejected(const ContentInfo* local_content,
                         const MediaContentDescription& answer_media_desc) {
  bool simulcast_offered = local_content &&
                           local_content->media_description() &&
                           local_content->media_description()->HasSimulcast();
  bool simulcast_answered = answer_media_desc.HasSimulcast();
  bool rids_supported = RtpExtension::FindHeaderExtensionByUri(
      answer_media_desc.rtp_header_extensions(), RtpExtension::kRidUri);
  return simulcast_offered && (!simulcast_answered || !rids_supported);
}

RTCError DisableSimulcastInSender(
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternal();
  if (parameters.encodings.size() <= 1) {
    return RTCError::OK();
  }

  std::vector<std::string> disabled_layers;
  std::transform(
      parameters.encodings.begin() + 1, parameters.encodings.end(),
      std::back_inserter(disabled_layers),
      [](const RtpEncodingParameters& encoding) { return encoding.rid; });
  return sender->DisableEncodingLayers(disabled_layers);
}

// Logic to decide if an m= section can be recycled. This means that the new
// m= section is not rejected, but the old local or remote m= section is
// rejected. |old_content_one| and |old_content_two| refer to the m= section
// of the old remote and old local descriptions in no particular order.
// We need to check both the old local and remote because either
// could be the most current from the latest negotation.
bool IsMediaSectionBeingRecycled(SdpType type,
                                 const ContentInfo& content,
                                 const ContentInfo* old_content_one,
                                 const ContentInfo* old_content_two) {
  return type == SdpType::kOffer && !content.rejected &&
         ((old_content_one && old_content_one->rejected) ||
          (old_content_two && old_content_two->rejected));
}

std::string GetStreamIdsString(rtc::ArrayView<const std::string> stream_ids) {
  std::string output = "streams=[";
  const char* separator = "";
  for (const auto& stream_id : stream_ids) {
    output.append(separator).append(stream_id);
    separator = ", ";
  }
  output.append("]");
  return output;
}

}  // namespace

void SdpOfferAnswerHandlerUnifiedPlan::OnOperationsChainEmpty() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed() || !update_negotiation_needed_on_empty_chain_)
    return;
  update_negotiation_needed_on_empty_chain_ = false;
  // Firing when chain is empty is only supported in Unified Plan to avoid Plan
  // B regressions. (In Plan B, onnegotiationneeded is already broken anyway, so
  // firing it even more might just be confusing.)
  UpdateNegotiationNeeded();
}

void SdpOfferAnswerHandlerUnifiedPlan::UpdateNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // In the spec, a task is queued here to run the following steps - this is
  // meant to ensure we do not fire onnegotiationneeded prematurely if multiple
  // changes are being made at once. In order to support Chromium's
  // implementation where the JavaScript representation of the PeerConnection
  // lives on a separate thread though, the queuing of a task is instead
  // performed by the PeerConnectionObserver posting from the signaling thread
  // to the JavaScript main thread that negotiation is needed. And because the
  // Operations Chain lives on the WebRTC signaling thread,
  // ShouldFireNegotiationNeededEvent() must be called before firing the event
  // to ensure the Operations Chain is still empty and the event has not been
  // invalidated.

  // If connection's [[IsClosed]] slot is true, abort these steps.
  if (pc_->IsClosed())
    return;

  // If connection's signaling state is not "stable", abort these steps.
  if (signaling_state() != PeerConnectionInterface::kStable)
    return;

  // NOTE
  // The negotiation-needed flag will be updated once the state transitions to
  // "stable", as part of the steps for setting an RTCSessionDescription.

  // If the result of checking if negotiation is needed is false, clear the
  // negotiation-needed flag by setting connection's [[NegotiationNeeded]] slot
  // to false, and abort these steps.
  bool is_negotiation_needed = CheckIfNegotiationIsNeeded();
  if (!is_negotiation_needed) {
    is_negotiation_needed_ = false;
    // Invalidate any negotiation needed event that may previosuly have been
    // generated.
    ++negotiation_needed_event_id_;
    return;
  }

  // If connection's [[NegotiationNeeded]] slot is already true, abort these
  // steps.
  if (is_negotiation_needed_)
    return;

  // Set connection's [[NegotiationNeeded]] slot to true.
  is_negotiation_needed_ = true;

  // Queue a task that runs the following steps:
  // If connection's [[IsClosed]] slot is true, abort these steps.
  // If connection's [[NegotiationNeeded]] slot is false, abort these steps.
  // Fire an event named negotiationneeded at connection.
  pc_->Observer()->OnRenegotiationNeeded();
  // Fire the spec-compliant version; when ShouldFireNegotiationNeededEvent() is
  // used in the task queued by the observer, this event will only fire when the
  // chain is empty.
  GenerateNegotiationNeededEvent();
}

bool SdpOfferAnswerHandlerUnifiedPlan::ShouldFireNegotiationNeededEvent(
    uint32_t event_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  // The event ID has been invalidated. Either negotiation is no longer needed
  // or a newer negotiation needed event has been generated.
  if (event_id != negotiation_needed_event_id_) {
    return false;
  }
  // The chain is no longer empty, update negotiation needed when it becomes
  // empty. This should generate a newer negotiation needed event, making this
  // one obsolete.
  if (!operations_chain_->IsEmpty()) {
    // Since we just suppressed an event that would have been fired, if
    // negotiation is still needed by the time the chain becomes empty again, we
    // must make sure to generate another event if negotiation is needed then.
    // This happens when |is_negotiation_needed_| goes from false to true, so we
    // set it to false until UpdateNegotiationNeeded() is called.
    is_negotiation_needed_ = false;
    update_negotiation_needed_on_empty_chain_ = true;
    return false;
  }
  // We must not fire if the signaling state is no longer "stable". If
  // negotiation is still needed when we return to "stable", a new negotiation
  // needed event will be generated, so this one can safely be suppressed.
  if (signaling_state_ != PeerConnectionInterface::kStable) {
    return false;
  }
  // All checks have passed - please fire "negotiationneeded" now!
  return true;
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::UpdateTransceiversAndDataChannels(
    cricket::ContentSource source,
    const SessionDescriptionInterface& new_session,
    const SessionDescriptionInterface* old_local_description,
    const SessionDescriptionInterface* old_remote_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  const cricket::ContentGroup* bundle_group = nullptr;
  if (new_session.GetType() == SdpType::kOffer) {
    auto bundle_group_or_error =
        GetEarlyBundleGroup(*new_session.description());
    if (!bundle_group_or_error.ok()) {
      return bundle_group_or_error.MoveError();
    }
    bundle_group = bundle_group_or_error.MoveValue();
  }

  const ContentInfos& new_contents = new_session.description()->contents();
  for (size_t i = 0; i < new_contents.size(); ++i) {
    const cricket::ContentInfo& new_content = new_contents[i];
    cricket::MediaType media_type = new_content.media_description()->type();
    mid_generator_.AddKnownId(new_content.name);
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      const cricket::ContentInfo* old_local_content = nullptr;
      if (old_local_description &&
          i < old_local_description->description()->contents().size()) {
        old_local_content =
            &old_local_description->description()->contents()[i];
      }
      const cricket::ContentInfo* old_remote_content = nullptr;
      if (old_remote_description &&
          i < old_remote_description->description()->contents().size()) {
        old_remote_content =
            &old_remote_description->description()->contents()[i];
      }
      auto transceiver_or_error =
          AssociateTransceiver(source, new_session.GetType(), i, new_content,
                               old_local_content, old_remote_content);
      if (!transceiver_or_error.ok()) {
        // In the case where a transceiver is rejected locally, we don't
        // expect to find a transceiver, but might find it in the case
        // where state is still "stopping", not "stopped".
        if (new_content.rejected) {
          continue;
        }
        return transceiver_or_error.MoveError();
      }
      auto transceiver = transceiver_or_error.MoveValue();
      RTCError error =
          UpdateTransceiverChannel(transceiver, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_DATA) {
      if (pc_->GetDataMid() && new_content.name != *(pc_->GetDataMid())) {
        // Ignore all but the first data section.
        RTC_LOG(LS_INFO) << "Ignoring data media section with MID="
                         << new_content.name;
        continue;
      }
      RTCError error = UpdateDataChannel(source, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_UNSUPPORTED) {
      RTC_LOG(LS_INFO) << "Ignoring unsupported media type";
    } else {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Unknown section type.");
    }
  }

  return RTCError::OK();
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::Rollback(SdpType desc_type) {
  auto state = signaling_state();
  if (state != PeerConnectionInterface::kHaveLocalOffer &&
      state != PeerConnectionInterface::kHaveRemoteOffer) {
    return RTCError(RTCErrorType::INVALID_STATE,
                    "Called in wrong signalingState: " +
                        GetSignalingStateString(signaling_state()));
  }
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_added_streams;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_removed_streams;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> removed_receivers;

  for (auto&& transceivers_stable_state_pair : transceivers()->StableStates()) {
    auto transceiver = transceivers_stable_state_pair.first;
    auto state = transceivers_stable_state_pair.second;

    if (state.remote_stream_ids()) {
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
      SetAssociatedRemoteStreams(transceiver->internal()->receiver_internal(),
                                 state.remote_stream_ids().value(),
                                 &added_streams, &removed_streams);
      all_added_streams.insert(all_added_streams.end(), added_streams.begin(),
                               added_streams.end());
      all_removed_streams.insert(all_removed_streams.end(),
                                 removed_streams.begin(),
                                 removed_streams.end());
      if (!state.has_m_section() && !state.newly_created()) {
        continue;
      }
    }

    RTC_DCHECK(transceiver->internal()->mid().has_value());
    DestroyTransceiverChannel(transceiver);

    if (signaling_state() == PeerConnectionInterface::kHaveRemoteOffer &&
        transceiver->receiver()) {
      removed_receivers.push_back(transceiver->receiver());
    }
    if (state.newly_created()) {
      if (transceiver->internal()->reused_for_addtrack()) {
        transceiver->internal()->set_created_by_addtrack(true);
      } else {
        transceivers()->Remove(transceiver);
      }
    }
    transceiver->internal()->sender_internal()->set_transport(nullptr);
    transceiver->internal()->receiver_internal()->set_transport(nullptr);
    transceiver->internal()->set_mid(state.mid());
    transceiver->internal()->set_mline_index(state.mline_index());
  }
  transport_controller()->RollbackTransports();
  if (have_pending_rtp_data_channel_) {
    DestroyDataChannelTransport();
    have_pending_rtp_data_channel_ = false;
  }
  transceivers()->DiscardStableStates();
  pending_local_description_.reset();
  pending_remote_description_.reset();
  ChangeSignalingState(PeerConnectionInterface::kStable);

  // Once all processing has finished, fire off callbacks.
  for (const auto& receiver : removed_receivers) {
    pc_->Observer()->OnRemoveTrack(receiver);
  }
  for (const auto& stream : all_added_streams) {
    pc_->Observer()->OnAddStream(stream);
  }
  for (const auto& stream : all_removed_streams) {
    pc_->Observer()->OnRemoveStream(stream);
  }

  // The assumption is that in case of implicit rollback UpdateNegotiationNeeded
  // gets called in SetRemoteDescription.
  if (desc_type == SdpType::kRollback) {
    UpdateNegotiationNeeded();
    if (is_negotiation_needed_) {
      // Legacy version.
      pc_->Observer()->OnRenegotiationNeeded();
      // Spec-compliant version; the event may get invalidated before firing.
      GenerateNegotiationNeededEvent();
    }
  }
  return RTCError::OK();
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
SdpOfferAnswerHandlerUnifiedPlan::AssociateTransceiver(
    cricket::ContentSource source,
    SdpType type,
    size_t mline_index,
    const ContentInfo& content,
    const ContentInfo* old_local_content,
    const ContentInfo* old_remote_content) {
  RTC_DCHECK(IsUnifiedPlan());
#if RTC_DCHECK_IS_ON
  // If this is an offer then the m= section might be recycled. If the m=
  // section is being recycled (defined as: rejected in the current local or
  // remote description and not rejected in new description), the transceiver
  // should have been removed by RemoveStoppedtransceivers()->
  if (IsMediaSectionBeingRecycled(type, content, old_local_content,
                                  old_remote_content)) {
    const std::string& old_mid =
        (old_local_content && old_local_content->rejected)
            ? old_local_content->name
            : old_remote_content->name;
    auto old_transceiver = transceivers()->FindByMid(old_mid);
    // The transceiver should be disassociated in RemoveStoppedTransceivers()
    RTC_DCHECK(!old_transceiver);
  }
#endif

  const MediaContentDescription* media_desc = content.media_description();
  auto transceiver = transceivers()->FindByMid(content.name);
  if (source == cricket::CS_LOCAL) {
    // Find the RtpTransceiver that corresponds to this m= section, using the
    // mapping between transceivers and m= section indices established when
    // creating the offer.
    if (!transceiver) {
      transceiver = transceivers()->FindByMLineIndex(mline_index);
    }
    if (!transceiver) {
      // This may happen normally when media sections are rejected.
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Transceiver not found based on m-line index");
    }
  } else {
    RTC_DCHECK_EQ(source, cricket::CS_REMOTE);
    // If the m= section is sendrecv or recvonly, and there are RtpTransceivers
    // of the same type...
    // When simulcast is requested, a transceiver cannot be associated because
    // AddTrack cannot be called to initialize it.
    if (!transceiver &&
        RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
        !media_desc->HasSimulcast()) {
      transceiver = FindAvailableTransceiverToReceive(media_desc->type());
    }
    // If no RtpTransceiver was found in the previous step, create one with a
    // recvonly direction.
    if (!transceiver) {
      RTC_LOG(LS_INFO) << "Adding "
                       << cricket::MediaTypeToString(media_desc->type())
                       << " transceiver for MID=" << content.name
                       << " at i=" << mline_index
                       << " in response to the remote description.";
      std::string sender_id = rtc::CreateRandomUuid();
      std::vector<RtpEncodingParameters> send_encodings =
          GetSendEncodingsFromRemoteDescription(*media_desc);
      auto sender = rtp_manager()->CreateSender(media_desc->type(), sender_id,
                                                nullptr, {}, send_encodings);
      std::string receiver_id;
      if (!media_desc->streams().empty()) {
        receiver_id = media_desc->streams()[0].id;
      } else {
        receiver_id = rtc::CreateRandomUuid();
      }
      auto receiver =
          rtp_manager()->CreateReceiver(media_desc->type(), receiver_id);
      transceiver = rtp_manager()->CreateAndAddTransceiver(sender, receiver);
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kRecvOnly);
      if (type == SdpType::kOffer) {
        transceivers()->StableState(transceiver)->set_newly_created();
      }
    }

    RTC_DCHECK(transceiver);

    // Check if the offer indicated simulcast but the answer rejected it.
    // This can happen when simulcast is not supported on the remote party.
    if (SimulcastIsRejected(old_local_content, *media_desc)) {
      RTC_HISTOGRAM_BOOLEAN(kSimulcastDisabled, true);
      RTCError error =
          DisableSimulcastInSender(transceiver->internal()->sender_internal());
      if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to remove rejected simulcast.";
        return std::move(error);
      }
    }
  }

  if (transceiver->media_type() != media_desc->type()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Transceiver type does not match media description type.");
  }

  if (media_desc->HasSimulcast()) {
    std::vector<SimulcastLayer> layers =
        source == cricket::CS_LOCAL
            ? media_desc->simulcast_description().send_layers().GetAllLayers()
            : media_desc->simulcast_description()
                  .receive_layers()
                  .GetAllLayers();
    RTCError error = UpdateSimulcastLayerStatusInSender(
        layers, transceiver->internal()->sender_internal());
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed updating status for simulcast layers.";
      return std::move(error);
    }
  }
  if (type == SdpType::kOffer) {
    bool state_changes = transceiver->internal()->mid() != content.name ||
                         transceiver->internal()->mline_index() != mline_index;
    if (state_changes) {
      transceivers()
          ->StableState(transceiver)
          ->SetMSectionIfUnset(transceiver->internal()->mid(),
                               transceiver->internal()->mline_index());
    }
  }
  // Associate the found or created RtpTransceiver with the m= section by
  // setting the value of the RtpTransceiver's mid property to the MID of the m=
  // section, and establish a mapping between the transceiver and the index of
  // the m= section.
  transceiver->internal()->set_mid(content.name);
  transceiver->internal()->set_mline_index(mline_index);
  return std::move(transceiver);
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::UpdateTransceiverChannel(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  RTC_DCHECK(IsUnifiedPlan());
  RTC_DCHECK(transceiver);
  cricket::ChannelInterface* channel = transceiver->internal()->channel();
  if (content.rejected) {
    if (channel) {
      transceiver->internal()->SetChannel(nullptr);
      DestroyChannelInterface(channel);
    }
  } else {
    if (!channel) {
      if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
        channel = CreateVoiceChannel(content.name);
      } else {
        RTC_DCHECK_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->media_type());
        channel = CreateVideoChannel(content.name);
      }
      if (!channel) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INTERNAL_ERROR,
            "Failed to create channel for mid=" + content.name);
      }
      transceiver->internal()->SetChannel(channel);
    }
  }
  return RTCError::OK();
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
SdpOfferAnswerHandlerUnifiedPlan::FindAvailableTransceiverToReceive(
    cricket::MediaType media_type) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  // From JSEP section 5.10 (Applying a Remote Description):
  // If the m= section is sendrecv or recvonly, and there are RtpTransceivers of
  // the same type that were added to the PeerConnection by addTrack and are not
  // associated with any m= section and are not stopped, find the first such
  // RtpTransceiver.
  for (auto transceiver : transceivers()->List()) {
    if (transceiver->media_type() == media_type &&
        transceiver->internal()->created_by_addtrack() && !transceiver->mid() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::HandleLegacyOfferOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  if (options.offer_to_receive_audio == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_audio > 1 is not supported.");
  }

  if (options.offer_to_receive_video == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_video > 1 is not supported.");
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandlerUnifiedPlan::RemoveStoppedTransceivers() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // 3.2.10.1: For each transceiver in the connection's set of transceivers
  //           run the following steps:
  if (!IsUnifiedPlan())
    return;
  // Traverse a copy of the transceiver list.
  auto transceiver_list = transceivers()->List();
  for (auto transceiver : transceiver_list) {
    // 3.2.10.1.1: If transceiver is stopped, associated with an m= section
    //             and the associated m= section is rejected in
    //             connection.[[CurrentLocalDescription]] or
    //             connection.[[CurrentRemoteDescription]], remove the
    //             transceiver from the connection's set of transceivers.
    if (!transceiver->stopped()) {
      continue;
    }
    const ContentInfo* local_content =
        FindMediaSectionForTransceiver(transceiver, local_description());
    const ContentInfo* remote_content =
        FindMediaSectionForTransceiver(transceiver, remote_description());
    if ((local_content && local_content->rejected) ||
        (remote_content && remote_content->rejected)) {
      RTC_LOG(LS_INFO) << "Dissociating transceiver"
                       << " since the media section is being recycled.";
      transceiver->internal()->set_mid(absl::nullopt);
      transceiver->internal()->set_mline_index(absl::nullopt);
      transceivers()->Remove(transceiver);
      continue;
    }
    if (!local_content && !remote_content) {
      // TODO(bugs.webrtc.org/11973): Consider if this should be removed already
      // See https://github.com/w3c/webrtc-pc/issues/2576
      RTC_LOG(LS_INFO)
          << "Dropping stopped transceiver that was never associated";
      transceivers()->Remove(transceiver);
      continue;
    }
  }
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::ValidateSessionDescriptionByPlan(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source) {
  // Ensure that each audio and video media section has at most one
  // "StreamParams". This will return an error if receiving a session
  // description from a "Plan B" endpoint which adds multiple tracks of the
  // same type. With Unified Plan, there can only be at most one track per
  // media section.
  for (const ContentInfo& content : sdesc->description()->contents()) {
    const MediaContentDescription& desc = *content.media_description();
    if ((desc.type() == cricket::MEDIA_TYPE_AUDIO ||
         desc.type() == cricket::MEDIA_TYPE_VIDEO) &&
        desc.streams().size() > 1u) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Media section has more than one track specified "
                           "with a=ssrc lines which is not supported with "
                           "Unified Plan.");
    }
  }
  return RTCError::OK();
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::ApplyLocalDescriptionByPlan(
    SdpType type,
    const SessionDescriptionInterface* old_local_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTCError error = UpdateTransceiversAndDataChannels(
      cricket::CS_LOCAL, *local_description(), old_local_description,
      remote_description());
  if (!error.ok()) {
    return error;
  }
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
  for (const auto& transceiver : transceivers()->List()) {
    if (transceiver->stopped()) {
      continue;
    }

    // 2.2.7.1.1.(6-9): Set sender and receiver's transport slots.
    // Note that code paths that don't set MID won't be able to use
    // information about DTLS transports.
    if (transceiver->mid()) {
      auto dtls_transport =
          transport_controller()->LookupDtlsTransportByMid(*transceiver->mid());
      transceiver->internal()->sender_internal()->set_transport(dtls_transport);
      transceiver->internal()->receiver_internal()->set_transport(
          dtls_transport);
    }

    const ContentInfo* content =
        FindMediaSectionForTransceiver(transceiver, local_description());
    if (!content) {
      continue;
    }
    const MediaContentDescription* media_desc = content->media_description();
    // 2.2.7.1.6: If description is of type "answer" or "pranswer", then run
    // the following steps:
    if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
      // 2.2.7.1.6.1: If direction is "sendonly" or "inactive", and
      // transceiver's [[FiredDirection]] slot is either "sendrecv" or
      // "recvonly", process the removal of a remote track for the media
      // description, given transceiver, removeList, and muteTracks.
      if (!RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
          (transceiver->internal()->fired_direction() &&
           RtpTransceiverDirectionHasRecv(
               *transceiver->internal()->fired_direction()))) {
        ProcessRemovalOfRemoteTrack(transceiver, &remove_list,
                                    &removed_streams);
      }
      // 2.2.7.1.6.2: Set transceiver's [[CurrentDirection]] and
      // [[FiredDirection]] slots to direction.
      transceiver->internal()->set_current_direction(media_desc->direction());
      transceiver->internal()->set_fired_direction(media_desc->direction());
    }
  }
  auto observer = pc_->Observer();
  for (const auto& transceiver : remove_list) {
    observer->OnRemoveTrack(transceiver->receiver());
  }
  for (const auto& stream : removed_streams) {
    observer->OnRemoveStream(stream);
  }

  // TEMP NOTE - this section is duplicated between Plan B and Unified Plan.
  // Consider breaking it out into a helper function.
  error = UpdateSessionState(type, cricket::CS_LOCAL,
                             local_description()->description());
  if (!error.ok()) {
    return error;
  }

  if (remote_description()) {
    // Now that we have a local description, we can push down remote candidates.
    UseCandidatesInSessionDescription(remote_description());
  }

  pending_ice_restarts_.clear();
  if (session_error() != SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, GetSessionErrorMsg());
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (IsSctpLike(pc_->data_channel_type()) && pc_->GetSctpSslRole(&role)) {
    data_channel_controller()->AllocateSctpSids(role);
  }

  // TEMP NOTE - end of duplicated section.

  for (const auto& transceiver : transceivers()->List()) {
    if (transceiver->stopped()) {
      continue;
    }
    const ContentInfo* content =
        FindMediaSectionForTransceiver(transceiver, local_description());
    if (!content) {
      continue;
    }
    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (content->rejected || !channel || channel->local_streams().empty()) {
      // 0 is a special value meaning "this sender has no associated send
      // stream". Need to call this so the sender won't attempt to configure
      // a no longer existing stream and run into DCHECKs in the lower
      // layers.
      transceiver->internal()->sender_internal()->SetSsrc(0);
    } else {
      // Get the StreamParams from the channel which could generate SSRCs.
      const std::vector<StreamParams>& streams = channel->local_streams();
      transceiver->internal()->sender_internal()->set_stream_ids(
          streams[0].stream_ids());
      transceiver->internal()->sender_internal()->SetSsrc(
          streams[0].first_ssrc());
    }
  }
  return error;
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::UpdateChannelsByPlan(
    SdpType type,
    const SessionDescriptionInterface* old_remote_description) {
  // Transport and Media channels will be created only when offer is set.
  return UpdateTransceiversAndDataChannels(
      cricket::CS_REMOTE, *remote_description(), local_description(),
      old_remote_description);
}

RTCError SdpOfferAnswerHandlerUnifiedPlan::ApplyRemoteDescriptionByPlan(
    SdpType type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
      now_receiving_transceivers;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
  for (const auto& transceiver : transceivers()->List()) {
    const ContentInfo* content =
        FindMediaSectionForTransceiver(transceiver, remote_description());
    if (!content) {
      continue;
    }
    const MediaContentDescription* media_desc = content->media_description();
    RtpTransceiverDirection local_direction =
        RtpTransceiverDirectionReversed(media_desc->direction());
    // Roughly the same as steps 2.2.8.6 of section 4.4.1.6 "Set the
    // RTCSessionDescription: Set the associated remote streams given
    // transceiver.[[Receiver]], msids, addList, and removeList".
    // https://w3c.github.io/webrtc-pc/#set-the-rtcsessiondescription
    if (RtpTransceiverDirectionHasRecv(local_direction)) {
      std::vector<std::string> stream_ids;
      if (!media_desc->streams().empty()) {
        // The remote description has signaled the stream IDs.
        stream_ids = media_desc->streams()[0].stream_ids();
      }
      transceivers()
          ->StableState(transceiver)
          ->SetRemoteStreamIdsIfUnset(transceiver->receiver()->stream_ids());

      RTC_LOG(LS_INFO) << "Processing the MSIDs for MID=" << content->name
                       << " (" << GetStreamIdsString(stream_ids) << ").";
      SetAssociatedRemoteStreams(transceiver->internal()->receiver_internal(),
                                 stream_ids, &added_streams, &removed_streams);
      // From the WebRTC specification, steps 2.2.8.5/6 of section 4.4.1.6
      // "Set the RTCSessionDescription: If direction is sendrecv or recvonly,
      // and transceiver's current direction is neither sendrecv nor recvonly,
      // process the addition of a remote track for the media description.
      if (!transceiver->fired_direction() ||
          !RtpTransceiverDirectionHasRecv(*transceiver->fired_direction())) {
        RTC_LOG(LS_INFO) << "Processing the addition of a remote track for MID="
                         << content->name << ".";
        now_receiving_transceivers.push_back(transceiver);
      }
    }
    // 2.2.8.1.9: If direction is "sendonly" or "inactive", and transceiver's
    // [[FiredDirection]] slot is either "sendrecv" or "recvonly", process the
    // removal of a remote track for the media description, given transceiver,
    // removeList, and muteTracks.
    if (!RtpTransceiverDirectionHasRecv(local_direction) &&
        (transceiver->fired_direction() &&
         RtpTransceiverDirectionHasRecv(*transceiver->fired_direction()))) {
      ProcessRemovalOfRemoteTrack(transceiver, &remove_list, &removed_streams);
    }
    // 2.2.8.1.10: Set transceiver's [[FiredDirection]] slot to direction.
    transceiver->internal()->set_fired_direction(local_direction);
    // 2.2.8.1.11: If description is of type "answer" or "pranswer", then run
    // the following steps:
    if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
      // 2.2.8.1.11.1: Set transceiver's [[CurrentDirection]] slot to
      // direction.
      transceiver->internal()->set_current_direction(local_direction);
      // 2.2.8.1.11.[3-6]: Set the transport internal slots.
      if (transceiver->mid()) {
        auto dtls_transport = transport_controller()->LookupDtlsTransportByMid(
            *transceiver->mid());
        transceiver->internal()->sender_internal()->set_transport(
            dtls_transport);
        transceiver->internal()->receiver_internal()->set_transport(
            dtls_transport);
      }
    }
    // 2.2.8.1.12: If the media description is rejected, and transceiver is
    // not already stopped, stop the RTCRtpTransceiver transceiver.
    if (content->rejected && !transceiver->stopped()) {
      RTC_LOG(LS_INFO) << "Stopping transceiver for MID=" << content->name
                       << " since the media section was rejected.";
      transceiver->internal()->StopTransceiverProcedure();
    }
    if (!content->rejected && RtpTransceiverDirectionHasRecv(local_direction)) {
      if (!media_desc->streams().empty() &&
          media_desc->streams()[0].has_ssrcs()) {
        uint32_t ssrc = media_desc->streams()[0].first_ssrc();
        transceiver->internal()->receiver_internal()->SetupMediaChannel(ssrc);
      } else {
        transceiver->internal()
            ->receiver_internal()
            ->SetupUnsignaledMediaChannel();
      }
    }
    // Once all processing has finished, fire off callbacks.
    auto observer = pc_->Observer();
    for (const auto& transceiver : now_receiving_transceivers) {
      pc_->stats()->AddTrack(transceiver->receiver()->track());
      observer->OnTrack(transceiver);
      observer->OnAddTrack(transceiver->receiver(),
                           transceiver->receiver()->streams());
    }
    for (const auto& stream : added_streams) {
      observer->OnAddStream(stream);
    }
    for (const auto& transceiver : remove_list) {
      observer->OnRemoveTrack(transceiver->receiver());
    }
    for (const auto& stream : removed_streams) {
      observer->OnRemoveStream(stream);
    }
  }

  const cricket::AudioContentDescription* audio_desc =
      GetFirstAudioContentDescription(remote_description()->description());
  const cricket::VideoContentDescription* video_desc =
      GetFirstVideoContentDescription(remote_description()->description());

  // Check if the descriptions include streams, just in case the peer supports
  // MSID, but doesn't indicate so with "a=msid-semantic".
  if (remote_description()->description()->msid_supported() ||
      (audio_desc && !audio_desc->streams().empty()) ||
      (video_desc && !video_desc->streams().empty())) {
    remote_peer_supports_msid_ = true;
  }

  // We wait to signal new streams until we finish processing the description,
  // since only at that point will new streams have all their tracks.
  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());
  return RTCError::OK();
}

void SdpOfferAnswerHandlerUnifiedPlan::SetLocalRollbackCompleteByPlan(
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer,
    const SessionDescriptionInterface* desc) {
  observer->OnSetLocalDescriptionComplete(Rollback(desc->GetType()));
}

bool SdpOfferAnswerHandlerUnifiedPlan::SetRemoteRollbackCompleteByPlan(
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer,
    const SessionDescriptionInterface* desc) {
  if (pc_->configuration()->enable_implicit_rollback) {
    if (desc->GetType() == SdpType::kOffer &&
        signaling_state() == PeerConnectionInterface::kHaveLocalOffer) {
      Rollback(desc->GetType());
    }
  }
  // Explicit rollback.
  if (desc->GetType() == SdpType::kRollback) {
    observer->OnSetRemoteDescriptionComplete(Rollback(desc->GetType()));
    return true;
  }
  return false;
}

void SdpOfferAnswerHandlerUnifiedPlan::CheckIfNegotiationIsNeededByPlan() {
  // Check if negotiation is needed. We must do this after informing the
  // observer that SetLocalDescription() has completed to ensure negotiation is
  // not needed prior to the promise resolving.
  RTC_DCHECK_RUN_ON(signaling_thread());
  bool was_negotiation_needed = is_negotiation_needed_;
  UpdateNegotiationNeeded();
  if (signaling_state() == PeerConnectionInterface::kStable &&
      was_negotiation_needed && is_negotiation_needed_) {
    // Legacy version.
    pc_->Observer()->OnRenegotiationNeeded();
    // Spec-compliant version; the event may get invalidated before firing.
    GenerateNegotiationNeededEvent();
  }
}

}  // namespace webrtc
