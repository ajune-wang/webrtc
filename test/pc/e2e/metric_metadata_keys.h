/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_METRIC_METADATA_KEYS_H_
#define TEST_PC_E2E_METRIC_METADATA_KEYS_H_

namespace webrtc {
namespace webrtc_pc_e2e {

class MetricMetadataKey {
 public:
  // Presented if applicable. Represents on peer with whom the metric is
  // associated.
  static constexpr char kPeerMetadataKey[] = "peer";
  // Presented if applicable. Represents sender of the media stream.
  static constexpr char kSenderMetadataKey[] = "sender";
  // Presented if applicable. Represents receiver of the media stream.
  static constexpr char kReceiverMetadataKey[] = "receiver";
  // Presented if applicable. Represents name of the audio stream.
  static constexpr char kAudioStreamMetadataKey[] = "audio_stream";
  // Presented if applicable. Represents name of the video stream.
  static constexpr char kVideoStreamMetadataKey[] = "video_stream";
  // Presented if applicable. Represents name of the sync group to which stream
  // belongs.
  static constexpr char kPeerSyncGroupMetadataKey[] = "peer_sync_group";

 private:
  MetricMetadataKey() = default;
};

class SampleMetadataKey {
 public:
  // Presented if applicable. Represents a frame ID with which data point is
  // associated.
  static constexpr char kFrameIdMetadataKey[] = "frame_id";

 private:
  SampleMetadataKey() = default;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_METRIC_METADATA_KEYS_H_
