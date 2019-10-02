/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_NETEQ_FACADE_IMPL_H_
#define MODULES_AUDIO_CODING_NETEQ_NETEQ_FACADE_IMPL_H_

#include "modules/audio_coding/neteq/neteq_controller.h"

#include "modules/audio_coding/neteq/buffer_level_filter.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"

namespace webrtc {

class NetEqFacadeImpl final : public NetEqFacade {
 public:
  NetEqFacadeImpl(const NetEqFacadeImpl&) = delete;
  NetEqFacadeImpl& operator=(const NetEqFacadeImpl&) = delete;
  ~NetEqFacadeImpl() override = default;
  NetEqFacadeImpl(PacketBuffer* packet_buffer,
                  BufferLevelFilter* buffer_level_filter,
                  DecoderDatabase* decoder_database,
                  StatisticsCalculator* statistics_calculator);

  bool ContainsDtxOrCngPacket() const override;
  size_t GetSpanSamples(size_t last_decoded_length,
                        size_t sample_rate,
                        bool count_dtx_waiting_time) const override;
  size_t NumSamplesInBuffer(size_t last_decoded_length) const override;
  size_t NumPacketsInBuffer() const override;
  void UpdateBufferLevelFilter(size_t buffer_size_samples,
                               int time_stretched_samples) override;
  void SetTargetBufferLevel(int target_buffer_level_packets) override;
  int GetFilteredBufferLevel() const override;
  void ReportRelativePacketArrivalDelay(size_t delay_ms) override;

 private:
  PacketBuffer* packet_buffer_;
  BufferLevelFilter* buffer_level_filter_;
  DecoderDatabase* decoder_database_;
  StatisticsCalculator* statistics_calculator_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_NETEQ_FACADE_IMPL_H_
