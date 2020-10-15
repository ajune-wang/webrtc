/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_OBJECT_MANAGER_H_
#define PC_RTP_OBJECT_MANAGER_H_

#include <string>
#include <vector>

#include "pc/transceiver_list.h"
#include "pc/usage_pattern.h"

namespace rtc {
class Thread;
}

namespace webrtc {

class PeerConnectionObserver;
class UsagePattern;
class StatsCollectorInterface;

struct RtpSenderInfo {
  RtpSenderInfo() : first_ssrc(0) {}
  RtpSenderInfo(const std::string& stream_id,
                const std::string sender_id,
                uint32_t ssrc)
      : stream_id(stream_id), sender_id(sender_id), first_ssrc(ssrc) {}
  bool operator==(const RtpSenderInfo& other) {
    return this->stream_id == other.stream_id &&
           this->sender_id == other.sender_id &&
           this->first_ssrc == other.first_ssrc;
  }
  std::string stream_id;
  std::string sender_id;
  // An RtpSender can have many SSRCs. The first one is used as a sort of ID
  // for communicating with the lower layers.
  uint32_t first_ssrc;
};

// The RtpObjectManager class is responsible for managing the lifetime
// and relationships between objects of type RtpSender, RtpReceiver and
// RtpTransceiver.
class RtpObjectManager : public RtpSenderBase::SetStreamsObserver,
                         public sigslot::has_slots<> {
 public:
  RtpObjectManager(bool is_unified_plan,
                   rtc::Thread* signaling_thread,
                   rtc::Thread* worker_thread,
                   cricket::ChannelManager* channel_manager,
                   UsagePattern* usage_pattern,
                   std::function<PeerConnectionObserver*()> observer_getter,
                   std::function<void()> on_negotiation_needed);

  // No move or copy permitted.
  RtpObjectManager(const RtpObjectManager&) = delete;
  RtpObjectManager& operator=(const RtpObjectManager&) = delete;

  // RtpSenderBase::SetStreamsObserver override.
  void OnSetStreams() override;

  // AddTrack implementation when Unified Plan is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackUnifiedPlan(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids);
  // AddTrack implementation when Plan B is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackPlanB(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids);

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  CreateSender(cricket::MediaType media_type,
               const std::string& id,
               rtc::scoped_refptr<MediaStreamTrackInterface> track,
               const std::vector<std::string>& stream_ids,
               const std::vector<RtpEncodingParameters>& send_encodings);

  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  CreateReceiver(cricket::MediaType media_type, const std::string& receiver_id);

  // Create a new RtpTransceiver of the given type and add it to the list of
  // transceivers.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  CreateAndAddTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  // Returns the first RtpTransceiver suitable for a newly added track, if such
  // transceiver is available.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindFirstTransceiverForAddedTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track);

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  GetSendersInternal() const;

  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  GetReceiversInternal() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAudioTransceiver() const;
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetVideoTransceiver() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetFirstAudioTransceiver() const;

  void AddAudioTrack(AudioTrackInterface* track, MediaStreamInterface* stream);
  void RemoveAudioTrack(AudioTrackInterface* track,
                        MediaStreamInterface* stream);
  void AddVideoTrack(VideoTrackInterface* track, MediaStreamInterface* stream);
  void RemoveVideoTrack(VideoTrackInterface* track,
                        MediaStreamInterface* stream);

  void CreateAudioReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpReceiverInterface> RemoveAndStopReceiver(
      const RtpSenderInfo& remote_sender_info) RTC_RUN_ON(signaling_thread());

  // Triggered when a remote sender has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers CreateAudioReceiver or CreateVideoReceiver.
  void OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                           MediaStreamInterface* stream,
                           cricket::MediaType media_type);

  // Triggered when a remote sender has been removed from a remote session
  // description. It removes the remote sender with id |sender_id| from a remote
  // MediaStream and triggers DestroyAudioReceiver or DestroyVideoReceiver.
  void OnRemoteSenderRemoved(const RtpSenderInfo& sender_info,
                             MediaStreamInterface* stream,
                             cricket::MediaType media_type);

  // Triggered when a local sender has been seen for the first time in a local
  // session description.
  // This method triggers CreateAudioSender or CreateVideoSender if the rtp
  // streams in the local SessionDescription can be mapped to a MediaStreamTrack
  // in a MediaStream in |local_streams_|
  void OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                          cricket::MediaType media_type);

  // Triggered when a local sender has been removed from a local session
  // description.
  // This method triggers DestroyAudioSender or DestroyVideoSender if a stream
  // has been removed from the local SessionDescription and the stream can be
  // mapped to a MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalSenderRemoved(const RtpSenderInfo& sender_info,
                            cricket::MediaType media_type);

  std::vector<RtpSenderInfo>* GetRemoteSenderInfos(
      cricket::MediaType media_type);
  std::vector<RtpSenderInfo>* GetLocalSenderInfos(
      cricket::MediaType media_type);
  const RtpSenderInfo* FindSenderInfo(const std::vector<RtpSenderInfo>& infos,
                                      const std::string& stream_id,
                                      const std::string sender_id) const;

  // Return the RtpSender with the given track attached.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderForTrack(MediaStreamTrackInterface* track) const;

  // Return the RtpSender with the given id, or null if none exists.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderById(const std::string& sender_id) const;

  // Return the RtpReceiver with the given id, or null if none exists.
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  FindReceiverById(const std::string& receiver_id) const;

  TransceiverList* transceivers() { return &transceivers_; }
  const TransceiverList* transceivers() const { return &transceivers_; }

  // Plan B helpers for getting the voice/video media channels for the single
  // audio/video transceiver, if it exists.
  cricket::VoiceMediaChannel* voice_media_channel() const;
  cricket::VideoMediaChannel* video_media_channel() const;

 private:
  rtc::Thread* signaling_thread() const { return signaling_thread_; }
  rtc::Thread* worker_thread() const { return worker_thread_; }
  cricket::ChannelManager* channel_manager() const { return channel_manager_; }
  bool IsUnifiedPlan() const { return is_unified_plan_; }
  void NoteUsageEvent(UsageEvent event) {
    usage_pattern_->NoteUsageEvent(event);
  }

  // Not sure how to provide this
  PeerConnectionObserver* Observer() const;
  void OnNegotiationNeeded();

  TransceiverList transceivers_;

  // These lists store sender info seen in local/remote descriptions.
  std::vector<RtpSenderInfo> remote_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> remote_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());

  const bool is_unified_plan_;
  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  cricket::ChannelManager* channel_manager_;
  UsagePattern* usage_pattern_;
  StatsCollectorInterface* stats_;
  std::function<PeerConnectionObserver*()> observer_getter_;
  std::function<void()> on_negotiation_needed_;
};

}  // namespace webrtc

#endif  // PC_RTP_OBJECT_MANAGER_H_
