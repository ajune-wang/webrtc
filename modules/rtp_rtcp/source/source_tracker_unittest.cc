/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/source_tracker.h"

#include <algorithm>
#include <list>
#include <random>
#include <set>
#include <utility>

#include "api/rtp_headers.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::Test;

constexpr size_t kIterationsCount = 200;
constexpr size_t kPacketInfosCountMax = 5;

class SourceTrackerTest : public Test {
 protected:
  SourceTrackerTest()
      : generator_(42), clock_(1000000000000ULL), tracker_(&clock_) {}

  void RunTest(uint32_t ssrcs_count, uint32_t csrcs_count) {
    ASSERT_GT(ssrcs_count, 0U);

    std::list<RtpSource> expected;

    ASSERT_THAT(tracker_.GetSources(), IsEmpty());

    for (size_t iteration = 0; iteration < kIterationsCount; ++iteration) {
      size_t infos_count = GenerateInfosCount(iteration);

      RtpPacketInfos::vector_type infos;
      for (size_t i = 0; i < infos_count; ++i) {
        const auto& ssrc = GenerateSsrc(ssrcs_count);
        const auto& csrcs = GenerateCsrcs(csrcs_count);
        const auto& sequence_number = GenerateSequenceNumber();
        const auto& rtp_timestamp = GenerateRtpTimestamp();
        const auto& audio_level = GenerateAudioLevel();
        const auto& receive_time_ms = GenerateReceiveTimeMs();

        infos.push_back(RtpPacketInfo(ssrc, csrcs, sequence_number,
                                      rtp_timestamp, audio_level,
                                      receive_time_ms));

        for (const auto& csrc : csrcs) {
          expected.push_front(RtpSource(clock_.TimeInMilliseconds(), csrc,
                                        RtpSourceType::CSRC, audio_level,
                                        rtp_timestamp));
        }

        expected.push_front(RtpSource(clock_.TimeInMilliseconds(), ssrc,
                                      RtpSourceType::SSRC, audio_level,
                                      rtp_timestamp));
      }

      tracker_.OnFrameDelivered(RtpPacketInfos(infos));

      clock_.AdvanceTimeMilliseconds(GenerateClockAdvanceTimeMilliseconds());

      PruneEntries(&expected);

      ASSERT_THAT(tracker_.GetSources(), ElementsAreArray(expected));
    }
  }

 private:
  size_t GenerateInfosCount(size_t iteration) {
    return std::uniform_int_distribution<size_t>(
        1, std::min(iteration + 1, kPacketInfosCountMax))(generator_);
  }

  uint32_t GenerateSsrc(uint32_t ssrcs_count) {
    return std::uniform_int_distribution<uint32_t>(1, ssrcs_count)(generator_);
  }

  std::vector<uint32_t> GenerateCsrcs(uint32_t csrcs_count) {
    std::vector<uint32_t> csrcs;
    for (size_t i = 1; i <= csrcs_count && csrcs.size() < kRtpCsrcSize; ++i) {
      if (std::bernoulli_distribution(0.5)(generator_)) {
        csrcs.push_back(i);
      }
    }

    return csrcs;
  }

  uint16_t GenerateSequenceNumber() {
    return std::uniform_int_distribution<uint16_t>()(generator_);
  }

  uint32_t GenerateRtpTimestamp() {
    return std::uniform_int_distribution<uint32_t>()(generator_);
  }

  absl::optional<uint8_t> GenerateAudioLevel() {
    if (std::bernoulli_distribution(0.25)(generator_)) {
      return absl::nullopt;
    }

    return std::uniform_int_distribution<uint8_t>()(generator_);
  }

  int64_t GenerateReceiveTimeMs() {
    return std::uniform_int_distribution<int64_t>()(generator_);
  }

  int64_t GenerateClockAdvanceTimeMilliseconds() {
    double roll = std::uniform_real_distribution<double>(0.0, 1.0)(generator_);

    if (roll < 0.05) {
      return 0;
    }

    if (roll < 0.08) {
      return SourceTracker::kTimeoutMs - 1;
    }

    if (roll < 0.11) {
      return SourceTracker::kTimeoutMs;
    }

    if (roll < 0.19) {
      return std::uniform_int_distribution<int64_t>(
          SourceTracker::kTimeoutMs,
          SourceTracker::kTimeoutMs * 1000)(generator_);
    }

    return std::uniform_int_distribution<int64_t>(
        1, SourceTracker::kTimeoutMs - 1)(generator_);
  }

  void PruneEntries(std::list<RtpSource>* list) {
    std::set<std::pair<RtpSourceType, uint32_t>> seen;

    auto prune_ms = clock_.TimeInMilliseconds() - SourceTracker::kTimeoutMs;

    auto it = list->begin();
    auto end = list->end();
    while (it != end) {
      auto next = it;
      ++next;

      auto key = std::make_pair(it->source_type(), it->source_id());
      if (!seen.insert(key).second || it->timestamp_ms() < prune_ms) {
        list->erase(it);
      }

      it = next;
    }
  }

  std::mt19937 generator_;
  SimulatedClock clock_;
  SourceTracker tracker_;
};

}  // namespace

TEST_F(SourceTrackerTest, OneSsrcAndZeroCsrcs) {
  RunTest(1, 0);
}

TEST_F(SourceTrackerTest, OneSsrcAndOneCsrc) {
  RunTest(1, 1);
}

TEST_F(SourceTrackerTest, OneSsrcAndFiveCsrcs) {
  RunTest(1, 5);
}

TEST_F(SourceTrackerTest, ThreeSsrcAndZeroCsrcs) {
  RunTest(3, 0);
}

TEST_F(SourceTrackerTest, ThreeSsrcAndOneCsrc) {
  RunTest(3, 1);
}

TEST_F(SourceTrackerTest, ThreeSsrcAndFiveCsrcs) {
  RunTest(3, 5);
}

}  // namespace webrtc
