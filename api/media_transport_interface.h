/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media transport.
//
// The goal is to refactor WebRTC code so that audio and video frames
// are sent / received through media transport interface. This will
// enable different media transport implementation, including QUIC-based
// media transport.

#ifndef API_MEDIA_TRANSPORT_INTERFACE_H_
#define API_MEDIA_TRANSPORT_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "api/rtcerror.h"
#include "common_types.h"  // NOLINT(build/include)
#include "p2p/base/packettransportinternal.h"

namespace webrtc {

// Represents Encoded audio frame in any encoding (type of encoding is opaque).
// To avoid copying of encoded data use move semantics when passing by value.
class MediaTransportEncodedAudioFrame {
 public:
  // Parameters:
  //     sampling_rate_hz : audio sampling rate, for example 48000
  //     starting_sample_index : starting sample index of the frame, i.e. how
  //                             many audio samples were before this frame since
  //                             the beginning of the call or beginning of time
  //                             in one channel (the starting point should not
  //                             matter for NetEq). In WebRTC it is used as a
  //                             timestamp of the frame.
  //     sample_count : number of audio samples in audio frame in 1 channel.
  //     sequence_number : sequence number of the frame in the order sent,
  //                       it is currently required by NetEq, but we can fix
  //                       NetEq, because starting_sample_index should be
  //                       enough.
  //     payload_type : opaque payload type. In RTP codepath payload type is
  //                    stored in RTP header. In other implementations it
  //                    should be simply passed through the wire -- it's
  //                    needed for decoder.
  //     encoded_data : string with opaque encoded data.
  //
  // NOTE: All parameters except starting_sample_index must be preserved
  // on the wire.
  //
  // TODO(sukhanov): Starting_sample_index is currently adjusted on the
  // receiver side in RTP path. Non-RTP implementations should preserve it.
  // For NetEq initial offset should not matter so we should consider fixing
  // RTP path.
  MediaTransportEncodedAudioFrame(uint32_t sampling_rate_hz,
                                  uint32_t starting_sample_index,
                                  uint32_t sample_count,
                                  uint32_t sequence_number,
                                  FrameType frame_type,
                                  uint8_t payload_type,
                                  std::string encoded_data)
      : sampling_rate_hz_(sampling_rate_hz),
        starting_sample_index_(starting_sample_index),
        sample_count_(sample_count),
        sequence_number_(sequence_number),
        frame_type_(frame_type),
        payload_type_(payload_type),
        encoded_data_(std::move(encoded_data)) {
    RTC_CHECK(frame_type == kAudioFrameSpeech || frame_type == kAudioFrameCN)
        << "Unexpected frame_type=" << frame_type;
  }

  // Getters.
  uint32_t sampling_rate_hz() const { return sampling_rate_hz_; }
  uint32_t starting_sample_index() const { return starting_sample_index_; }
  uint32_t sample_count() const { return sample_count_; }

  // TODO(sukhanov): Refactor NetEq so we don't need sequence number.
  // Having sample_index and sample_count should be enough.
  uint32_t sequence_number() const { return sequence_number_; }

  uint8_t payload_type() const { return payload_type_; }
  FrameType frame_type() const { return frame_type_; }

  rtc::ArrayView<const char>encoded_data() const { return encoded_data_; }

 private:
  const uint32_t sampling_rate_hz_;
  const uint32_t starting_sample_index_;
  const uint32_t sample_count_;
  const uint32_t sequence_number_;

  const FrameType frame_type_;

  // TODO(sukhanov): Consider enumerating allowed encodings and store enum
  // instead of uint payload_type.
  const uint8_t payload_type_;

  const std::string encoded_data_;
};

// Interface for receiving encoded audio frames from MediaTransportInterface
// implementations.
class MediaTransportAudioSinkInterface {
 public:
  // Called when new encoded audio frame is received.
  virtual void OnData(uint64_t channel_id,
                      const MediaTransportEncodedAudioFrame& frame) = 0;

 protected:
  virtual ~MediaTransportAudioSinkInterface() = default;
};

// Media transport interface for sending / receiving encoded audio/video frames
// and receiving bandwidth estimate update from congestion control.
class MediaTransportInterface {
 public:
  virtual RTCError SendAudioFrame(
      uint64_t channel_id,
      const MediaTransportEncodedAudioFrame& frame) = 0;

  // TODO(sukhanov): We probably do not need multiple sinks, but it's
  // easier to test and debug if multiple sinks are supported.
  // An alternative approach would be to have a SinkFanout<T>, and have
  // SinkFanout<T>::Register(T), SinkFanout<T>::Unregister<T>, and we could use
  // it as an instance variable inside the media transport interface
  // implementation (and avoid redundant handling in different implementations).
  // All sinks should be unregistered by the time media transport is destroyed.
  virtual RTCError RegisterAudioSink(
      MediaTransportAudioSinkInterface* sink) = 0;

  virtual RTCError UnregisterAudioSink(
      MediaTransportAudioSinkInterface* sink) = 0;

  // TODO(sukhanov): RtcEventLogs.
  // TODO(sukhanov): Video interfaces.
  // TODO(sukhanov): Bandwidth updates.

 protected:
  virtual ~MediaTransportInterface() = default;
};

// If media transport factory is set in peer connection factory, it will be
// used to create media transport for sending/receiving encoded frames and
// this transport will be used instead of default RTP/SRTP transport.
//
// Currently Media Transport negotiation is not supported in SDP.
// If application is using media transport, it must negotiate it before
// setting media transport factory in peer connection.
class MediaTransportFactory {
 public:
  virtual ~MediaTransportFactory() = default;

  // Creates media transport.
  // - Does not take ownership of packet_transport or network_thread.
  // - Does not support group calls, in 1:1 call one side must set
  //   is_caller = true and another is_caller = false.
  virtual RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
  CreateMediaTransport(rtc::PacketTransportInternal* packet_transport,
                       rtc::Thread* network_thread,
                       bool is_caller) = 0;
};

}  // namespace webrtc

#endif  // API_MEDIA_TRANSPORT_INTERFACE_H_
