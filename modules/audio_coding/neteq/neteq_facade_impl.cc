/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/neteq_facade_impl.h"

namespace webrtc {

NetEqFacadeImpl::NetEqFacadeImpl(PacketBuffer* packet_buffer,
                                 BufferLevelFilter* buffer_level_filter,
                                 DecoderDatabase* decoder_database,
                                 StatisticsCalculator* statistics_calculator)
    : packet_buffer_(packet_buffer),
      buffer_level_filter_(buffer_level_filter),
      decoder_database_(decoder_database),
      statistics_calculator_(statistics_calculator) {}

bool NetEqFacadeImpl::ContainsDtxOrCngPacket() const {
  return packet_buffer_->ContainsDtxOrCngPacket(decoder_database_);
}

size_t NetEqFacadeImpl::GetSpanSamples(size_t last_decoded_length,
                                       size_t sample_rate,
                                       bool count_dtx_waiting_time) const {
  return packet_buffer_->GetSpanSamples(last_decoded_length, sample_rate,
                                        count_dtx_waiting_time);
}

size_t NetEqFacadeImpl::NumSamplesInBuffer(size_t last_decoded_length) const {
  return packet_buffer_->NumSamplesInBuffer(last_decoded_length);
}

void NetEqFacadeImpl::UpdateTargetBufferFilter(size_t buffer_size_samples,
                                               int time_stretched_samples) {
  buffer_level_filter_->Update(buffer_size_samples, time_stretched_samples);
}

void NetEqFacadeImpl::SetTargetBufferLevel(int target_buffer_level_packets) {
  buffer_level_filter_->SetTargetBufferLevel(target_buffer_level_packets);
}

void NetEqFacadeImpl::ReportRelativePacketArrivalDelay(int delay_ms) {
  statistics_calculator_->RelativePacketArrivalDelay(delay_ms);
}
}  // namespace webrtc
