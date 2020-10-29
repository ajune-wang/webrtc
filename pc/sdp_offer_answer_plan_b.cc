/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_offer_answer_plan_b.h"

namespace webrtc {

namespace {

// If the direction is "recvonly" or "inactive", treat the description
// as containing no streams.
// See: https://code.google.com/p/webrtc/issues/detail?id=5054
// TEMP NOTE: Duplicated with sdp_offer_answer.cc
std::vector<cricket::StreamParams> GetActiveStreams(
    const cricket::MediaContentDescription* desc) {
  return RtpTransceiverDirectionHasSend(desc->direction())
             ? desc->streams()
             : std::vector<cricket::StreamParams>();
}

}  // namespace

void SdpOfferAnswerHandlerPlanB::OnOperationsChainEmpty() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed() || !update_negotiation_needed_on_empty_chain_)
    return;
  update_negotiation_needed_on_empty_chain_ = false;
  // Firing when chain is empty is only supported in Unified Plan to avoid Plan
  // B regressions. (In Plan B, onnegotiationneeded is already broken anyway, so
  // firing it even more might just be confusing.)
}

void SdpOfferAnswerHandlerPlanB::UpdateNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  pc_->Observer()->OnRenegotiationNeeded();
  GenerateNegotiationNeededEvent();
  return;
}

bool SdpOfferAnswerHandlerPlanB::ShouldFireNegotiationNeededEvent(
    uint32_t event_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsUnifiedPlan());
  return true;
}

RTCError SdpOfferAnswerHandlerPlanB::ApplyLocalDescriptionByPlan(
    SdpType type,
    const SessionDescriptionInterface* old_local_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Media channels will be created only when offer is set. These may use new
  // transports just created by PushdownTransportDescription.
  if (type == SdpType::kOffer) {
    // TODO(bugs.webrtc.org/4676) - Handle CreateChannel failure, as new local
    // description is applied. Restore back to old description.
    RTCError error = CreateChannels(*local_description()->description());
    if (!error.ok()) {
      return error;
    }
  }
  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(local_description()->description());

  RTCError error = UpdateSessionState(type, cricket::CS_LOCAL,
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
  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(local_description()->description());
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
    } else {
      const cricket::AudioContentDescription* audio_desc =
          audio_content->media_description()->as_audio();
      UpdateLocalSenders(audio_desc->streams(), audio_desc->type());
    }
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(local_description()->description());
  if (video_content) {
    if (video_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
    } else {
      const cricket::VideoContentDescription* video_desc =
          video_content->media_description()->as_video();
      UpdateLocalSenders(video_desc->streams(), video_desc->type());
    }
  }
  return error;
}
RTCError SdpOfferAnswerHandlerPlanB::UpdateChannelsByPlan(
    SdpType type,
    const SessionDescriptionInterface* old_remote_description) {
  // Transport and Media channels will be created only when offer is set.
  // Media channels will be created only when offer is set. These may use new
  // transports just created by PushdownTransportDescription.
  if (type == SdpType::kOffer) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local
    // description is applied. Restore back to old description.
    RTCError error = CreateChannels(*remote_description()->description());
    if (!error.ok()) {
      return error;
    }
  }
  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(remote_description()->description());
  return RTCError::OK();
}

RTCError SdpOfferAnswerHandlerPlanB::ApplyRemoteDescriptionByPlan(
    SdpType type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(remote_description()->description());
  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(remote_description()->description());
  const cricket::AudioContentDescription* audio_desc =
      GetFirstAudioContentDescription(remote_description()->description());
  const cricket::VideoContentDescription* video_desc =
      GetFirstVideoContentDescription(remote_description()->description());
  const cricket::RtpDataContentDescription* rtp_data_desc =
      GetFirstRtpDataContentDescription(remote_description()->description());

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

  // TODO(steveanton): When removing RTP senders/receivers in response to a
  // rejected media section, there is some cleanup logic that expects the
  // voice/ video channel to still be set. But in this method the voice/video
  // channel would have been destroyed by the SetRemoteDescription caller
  // above so the cleanup that relies on them fails to run. The RemoveSenders
  // calls should be moved to right before the DestroyChannel calls to fix
  // this.

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
    } else {
      bool default_audio_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(audio_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(audio_desc),
                              default_audio_track_needed, audio_desc->type(),
                              new_streams);
    }
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  if (video_content) {
    if (video_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
    } else {
      bool default_video_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(video_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(video_desc),
                              default_video_track_needed, video_desc->type(),
                              new_streams);
    }
  }

  // If this is an RTP data transport, update the DataChannels with the
  // information from the remote peer.
  if (rtp_data_desc) {
    data_channel_controller()->UpdateRemoteRtpDataChannels(
        GetActiveStreams(rtp_data_desc));
  }

  // Iterate new_streams and notify the observer about new MediaStreams.
  auto observer = pc_->Observer();
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    pc_->stats()->AddStream(new_stream);
    observer->OnAddStream(rtc::scoped_refptr<MediaStreamInterface>(new_stream));
  }

  UpdateEndedRemoteMediaStreams();
  return RTCError::OK();
}

void SdpOfferAnswerHandlerPlanB::SetLocalRollbackCompleteByPlan(
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer,
    const SessionDescriptionInterface* desc) {
  observer->OnSetLocalDescriptionComplete(RTCError(
      RTCErrorType::UNSUPPORTED_OPERATION, "Rollback not supported in Plan B"));
}

bool SdpOfferAnswerHandlerPlanB::SetRemoteRollbackCompleteByPlan(
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer,
    const SessionDescriptionInterface* desc) {
  if (desc->GetType() == SdpType::kRollback) {
    observer->OnSetRemoteDescriptionComplete(
        RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                 "Rollback not supported in Plan B"));
    return true;
  }
  return false;
}

}  // namespace webrtc
