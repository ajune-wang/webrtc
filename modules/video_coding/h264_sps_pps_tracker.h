/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_
#define MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class VCMPacket;

namespace video_coding {

class H264SpsPpsTracker {
 public:
  enum PacketAction { kInsert, kDrop, kRequestKeyframe };

  H264SpsPpsTracker();
  ~H264SpsPpsTracker();

  PacketAction CopyAndFixBitstream(VCMPacket* packet);

  void InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                         const std::vector<uint8_t>& pps);

 private:
  struct PpsInfo {
    PpsInfo();
    PpsInfo(PpsInfo&& rhs);
    PpsInfo& operator=(PpsInfo&& rhs);
    ~PpsInfo();

    int sps_id = -1;
    absl::optional<uint16_t> rtp_seq_num;
    absl::optional<uint32_t> rtp_timestamp;
    std::vector<uint8_t> data;
  };

  struct SpsInfo {
    SpsInfo();
    SpsInfo(SpsInfo&& rhs);
    SpsInfo& operator=(SpsInfo&& rhs);
    ~SpsInfo();

    int width = -1;
    int height = -1;
    absl::optional<uint16_t> rtp_seq_num;
    absl::optional<uint32_t> rtp_timestamp;
    std::vector<uint8_t> data;
  };

  void StoreInBandSpsPps(const VCMPacket& packet);
  void StoreSps(const uint8_t* data,
                size_t data_size,
                uint32_t sps_id,
                uint32_t width,
                uint32_t height,
                absl::optional<uint16_t> rtp_seq_num,
                absl::optional<uint32_t> rtp_timestamp);
  void StorePps(const uint8_t* data,
                size_t data_size,
                uint32_t sps_id,
                uint32_t pps_id,
                absl::optional<uint16_t> rtp_seq_num,
                absl::optional<uint32_t> rtp_timestamp);
  bool IsContinuousSeqNum(const SpsInfo& sps,
                          const PpsInfo& pps,
                          const VCMPacket& packet);
  bool IsSameTimestamp(const SpsInfo& sps,
                       const PpsInfo& pps,
                       const VCMPacket& packet);
  std::map<uint32_t, PpsInfo> pps_data_;
  std::map<uint32_t, SpsInfo> sps_data_;

  const bool insert_inband_sps_pps_before_idr_;
  const bool reset_end_of_frame_flag_on_nonvlc_packet_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_
