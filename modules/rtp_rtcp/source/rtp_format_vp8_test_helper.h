/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the class RtpFormatVp8TestHelper. The class is
// responsible for setting up a fake VP8 bitstream according to the
// RTPVideoHeaderVP8 header, and partition information. After initialization,
// an RTPFragmentationHeader is provided so that the tester can create a
// packetizer. The packetizer can then be provided to this helper class, which
// will then extract all packets and compare to the expected outcome.

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_TEST_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_TEST_HELPER_H_

#include "api/array_view.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/buffer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

namespace test {

class RtpFormatVp8TestHelper {
 public:
  RtpFormatVp8TestHelper(const RTPVideoHeaderVP8* hdr, size_t payload_len);
  ~RtpFormatVp8TestHelper();
  void GetAllPacketsAndCheck(RtpPacketizerVp8* packetizer,
                             rtc::ArrayView<const size_t> expected_sizes);

  rtc::ArrayView<const uint8_t> payload() const { return payload_; }
  size_t payload_size() const { return payload_.size(); }

 private:
  void CheckHeader(bool first);
  void CheckPictureID();
  void CheckTl0PicIdx();
  void CheckTIDAndKeyIdx();
  void CheckPayload();
  void CheckLast(bool last) const;

  const RTPVideoHeaderVP8* const hdr_info_;
  rtc::Buffer payload_;

  RtpPacketToSend packet_;
  const uint8_t* data_ptr_;
  int payload_start_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpFormatVp8TestHelper);
};

}  // namespace test

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_TEST_HELPER_H_
