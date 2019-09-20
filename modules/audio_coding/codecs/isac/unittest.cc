/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <numeric>
#include <vector>

#include "modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"
#include "modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "rtc_base/buffer.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

namespace {

class BoundedCapacityChannel final {
 public:
  BoundedCapacityChannel(int sample_rate_hz, int rate_bits_per_second)
      : current_time_rtp_(0),
        channel_rate_bytes_per_sample_(rate_bits_per_second /
                                       (8.0 * sample_rate_hz)) {}

  // Simulate sending the given number of bytes at the given RTP time. Returns
  // the new current RTP time after the sending is done.
  int Send(int send_time_rtp, int nbytes) {
    current_time_rtp_ = std::max(current_time_rtp_, send_time_rtp) +
                        nbytes / channel_rate_bytes_per_sample_;
    return current_time_rtp_;
  }

 private:
  int current_time_rtp_;
  // The somewhat strange unit for channel rate, bytes per sample, is because
  // RTP time is measured in samples:
  const double channel_rate_bytes_per_sample_;
};

enum class IsacType { Fix, Float };

std::ostream& operator<<(std::ostream& os, IsacType t) {
  os << (t == IsacType::Fix ? "fix" : "float");
  return os;
}

struct IsacTestParam {
  IsacType isac_type;
  bool adaptive;
  int channel_rate_bits_per_second;
  int sample_rate_hz;
  int frame_size_ms;

  friend std::ostream& operator<<(std::ostream& os, const IsacTestParam& itp) {
    os << '{' << itp.isac_type << ','
       << (itp.adaptive ? "adaptive" : "nonadaptive") << ','
       << itp.channel_rate_bits_per_second << ',' << itp.sample_rate_hz << ','
       << itp.frame_size_ms << '}';
    return os;
  }
};

class IsacCommonTest : public ::testing::TestWithParam<IsacTestParam> {};

}  // namespace

std::vector<IsacTestParam> TestCases() {
  static const IsacType types[] = {IsacType::Fix, IsacType::Float};
  static const bool adaptives[] = {true, false};
  static const int channel_rates[] = {12000, 15000, 19000, 22000};
  static const int sample_rates[] = {16000, 32000};
  static const int frame_sizes[] = {30, 60};
  std::vector<IsacTestParam> cases;
  for (IsacType type : types)
    for (bool adaptive : adaptives)
      for (int channel_rate : channel_rates)
        for (int sample_rate : sample_rates)
          if (!(type == IsacType::Fix && sample_rate == 32000))
            for (int frame_size : frame_sizes)
              if (!(sample_rate == 32000 && frame_size == 60))
                cases.push_back(
                    {type, adaptive, channel_rate, sample_rate, frame_size});
  return cases;
}

INSTANTIATE_TEST_SUITE_P(, IsacCommonTest, ::testing::ValuesIn(TestCases()));

}  // namespace webrtc
