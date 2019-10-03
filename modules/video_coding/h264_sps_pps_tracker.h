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
    size_t size = 0;
    uint16_t rtp_seq_num;
    bool is_out_of_band = false;
    std::unique_ptr<uint8_t[]> data;
  };

  struct SpsInfo {
    SpsInfo();
    SpsInfo(SpsInfo&& rhs);
    SpsInfo& operator=(SpsInfo&& rhs);
    ~SpsInfo();

    size_t size = 0;
    int width = -1;
    int height = -1;
    uint16_t rtp_seq_num;
    bool is_out_of_band = false;
    std::unique_ptr<uint8_t[]> data;
  };

  void ParseAndStoreSpsPps(const VCMPacket& packet);
  void ParseAndStoreSps(const uint8_t* nalu_ptr,
                        size_t nalu_size_bytes,
                        bool is_out_of_band,
                        uint16_t rtp_seq_num);
  void ParseAndStorePps(const uint8_t* nalu_ptr,
                        size_t nalu_size_bytes,
                        bool is_out_of_band,
                        uint16_t rtp_seq_num);
  bool IsContinuousSeqNum(const SpsInfo& sps,
                          const PpsInfo& pps,
                          const VCMPacket& packet);

  std::map<uint32_t, PpsInfo> pps_data_;
  std::map<uint32_t, SpsInfo> sps_data_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_
