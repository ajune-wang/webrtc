/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_H265_PARAMETER_SETS_TRACKER_H_
#define MODULES_VIDEO_CODING_H265_PARAMETER_SETS_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "api/array_view.h"
#include "common_video/h265/h265_bitstream_parser.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
namespace video_coding {

// This is used on H.265 sender side to ensure we are always sending
// bitstream that has parameter set NALUs enclosed into the H.265 IRAP frames.
// Unlike H.264, the tracker is not intended to be used by receiver side
// for attempt to fix received bitstream. H.265 receiver must always issue key
// frame request if parameter set is not part of IRAP picture.
// For more details, refer to:
// https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-hevc-webrtc-01
class H265ParameterSetsTracker {
 public:
  enum PacketAction {
    kInsert,
    kDropAud,
    kRequestKeyframe,
    kPassThrough,
    kInsertAndDropAud
  };
  struct FixedBitstream {
    PacketAction action;
    rtc::CopyOnWriteBuffer bitstream;
  };

  H265ParameterSetsTracker();
  ~H265ParameterSetsTracker();

  // Keeps track of incoming bitstream and insert VPS/SPS/PPS before the VCL
  // layer NALUs when needed.
  // Once VPS/SPS/PPS is detected in the bitstream, it will be recorded, and
  // if an IRAP picture is passed in without associated VPS/SPS/PPS in the
  // bitstream, will return the fixed bitstream with action set to kInsert; If
  // the incoming bitstream already contains neccessary parameter sets, or
  // incoming bitstream does not contain IRAP pictures, the returned
  // FixedBistream's |bitstream member| is not set, and |action| will be set to
  // kPassThrough; If the incoming bitstream needs to be fixed but corresponding
  // parameter set is not found, the returned FixedBitstream will get |action|
  // set to kRequestkeyframe, and its |bitstream| member will not be set.
  // Also if AUD NALU exists in the bitstream, we will remove the first AUD and
  // return fixed bitstream with |action| set to either kDropAud or
  // kInsertAndDropAud depending on whether we also insert parameter sets into
  // it. We do this to avoid inserting parameter sets between the first AUD and
  // subsequent VCL NALU, and we don't need to stream the AUD.
  FixedBitstream MaybeFixBitstream(rtc::ArrayView<const uint8_t> bitstream);

 private:
  struct PpsInfo {
    PpsInfo();
    PpsInfo(PpsInfo&& rhs);
    PpsInfo& operator=(PpsInfo&& rhs);
    ~PpsInfo();

    int sps_id = -1;
    size_t size = 0;
    std::unique_ptr<uint8_t[]> data;
  };

  struct SpsInfo {
    SpsInfo();
    SpsInfo(SpsInfo&& rhs);
    SpsInfo& operator=(SpsInfo&& rhs);
    ~SpsInfo();

    int vps_id = -1;
    size_t size = 0;
    std::unique_ptr<uint8_t[]> data;
  };

  struct VpsInfo {
    VpsInfo();
    VpsInfo(VpsInfo&& rhs);
    VpsInfo& operator=(VpsInfo&& rhs);
    ~VpsInfo();

    size_t size = 0;
    std::unique_ptr<uint8_t[]> data;
  };

  H265BitstreamParser parser_;
  webrtc::flat_map<uint32_t, PpsInfo> pps_data_;
  webrtc::flat_map<uint32_t, SpsInfo> sps_data_;
  webrtc::flat_map<uint32_t, VpsInfo> vps_data_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H265_PARAMETER_SETS_TRACKER_H_
