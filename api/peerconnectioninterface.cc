/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/peerconnectioninterface.h"

namespace webrtc {

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnectionInterface::AddTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented");
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnectionInterface::AddTrack(
    MediaStreamTrackInterface* track,
    std::vector<MediaStreamInterface*> streams) {
  // Default implementation provided so downstream implementations can remove
  // this.
  return nullptr;
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(cricket::MediaType media_type) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(cricket::MediaType media_type,
                                        const RtpTransceiverInit& init) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnectionInterface::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  return rtc::scoped_refptr<RtpSenderInterface>();
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnectionInterface::GetSenders() const {
  return std::vector<rtc::scoped_refptr<RtpSenderInterface>>();
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnectionInterface::GetReceivers() const {
  return std::vector<rtc::scoped_refptr<RtpReceiverInterface>>();
}

std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::GetTransceivers() const {
  return {};
}

const SessionDescriptionInterface*
PeerConnectionInterface::current_local_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::current_remote_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::pending_local_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::pending_remote_description() const {
  return nullptr;
}

PeerConnectionInterface::RTCConfiguration GetConfiguration() {
  return PeerConnectionInterface::RTCConfiguration();
}

bool PeerConnectionInterface::SetConfiguration(
    const PeerConnectionInterface::RTCConfiguration& config,
    RTCError* error) {
  return false;
}

bool PeerConnectionInterface::SetConfiguration(
    const PeerConnectionInterface::RTCConfiguration& config) {
  return false;
}

bool PeerConnectionInterface::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  return false;
}

RTCError PeerConnectionInterface::SetBitrate(const BitrateSettings& bitrate) {
  BitrateParameters bitrate_parameters;
  bitrate_parameters.min_bitrate_bps = bitrate.min_bitrate_bps;
  bitrate_parameters.current_bitrate_bps = bitrate.start_bitrate_bps;
  bitrate_parameters.max_bitrate_bps = bitrate.max_bitrate_bps;
  return SetBitrate(bitrate_parameters);
}

RTCError PeerConnectionInterface::SetBitrate(
    const BitrateParameters& bitrate_parameters) {
  BitrateSettings bitrate;
  bitrate.min_bitrate_bps = bitrate_parameters.min_bitrate_bps;
  bitrate.start_bitrate_bps = bitrate_parameters.current_bitrate_bps;
  bitrate.max_bitrate_bps = bitrate_parameters.max_bitrate_bps;
  return SetBitrate(bitrate);
}

bool PeerConnectionInterface::StartRtcEventLog(rtc::PlatformFile file,
                                               int64_t max_size_bytes) {
  return false;
}

bool PeerConnectionInterface::StartRtcEventLog(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  return false;
}

}  // namespace webrtc
