/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_NETEQ_CONTROLLER_H_
#define MODULES_AUDIO_CODING_NETEQ_NETEQ_CONTROLLER_H_

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/defines.h"
#include "modules/audio_coding/neteq/tick_timer.h"

namespace webrtc {

// This interface provides all of the functionality from the rest of NetEq that
// is needed by NetEqController.
class NetEqFacade {
 public:
  NetEqFacade(const NetEqFacade&) = delete;
  NetEqFacade& operator=(const NetEqFacade&) = delete;

  virtual ~NetEqFacade();

  // PacketBuffer functions.

  // Returns true if the packet buffer contains any DTX or CNG packets.
  virtual bool ContainsDtxOrCngPacket() const = 0;

  // Returns the total duration in samples that the packets in the buffer spans
  // across.
  virtual size_t GetSpanSamples(size_t last_decoded_length,
                                size_t sample_rate,
                                bool count_dtx_waiting_time) const = 0;

  // Returns the number of samples in the buffer, including samples carried in
  // duplicate and redundant packets.
  virtual size_t NumSamplesInBuffer(size_t last_decoded_length) const = 0;

  // Returns the number of packets in the buffer, including duplicates and
  // redundant packets.
  virtual size_t NumPacketsInBuffer() const = 0;

  // Static method returning true if |timestamp| is older than |timestamp_limit|
  // but less than |horizon_samples| behind |timestamp_limit|. For instance,
  // with timestamp_limit = 100 and horizon_samples = 10, a timestamp in the
  // range (90, 100) is considered obsolete, and will yield true.
  // Setting |horizon_samples| to 0 is the same as setting it to 2^31, i.e.,
  // half the 32-bit timestamp range.
  static bool IsObsoleteTimestamp(uint32_t timestamp,
                                  uint32_t timestamp_limit,
                                  uint32_t horizon_samples);

  // BufferLevelFilter functions.

  // Updates the buffer level filter. Current buffer size is
  // |buffer_size_packets| (Q0). |time_stretched_samples| is subtracted from the
  // filtered value (thus bypassing the filter operation).
  virtual void UpdateBufferLevelFilter(size_t buffer_size_samples,
                                       int time_stretched_samples) = 0;

  // Set the current target buffer level in number of packets (obtained from
  // DelayManager::base_target_level()). Used to select the appropriate
  // filter coefficient.
  virtual void SetTargetBufferLevel(int target_buffer_level_packets) = 0;

  // Get the current filtered buffer level in Q8.
  virtual int GetFilteredBufferLevel() const = 0;

  // StatisticsCalculator functions.

  // Reports that a received packet was delayed by |delay_ms| milliseconds.
  virtual void ReportRelativePacketArrivalDelay(size_t delay_ms) = 0;

 protected:
  NetEqFacade();
};

class NetEqController {
 public:
  // This struct is used by NetEqControllerFactory to create a NetEqController.
  struct Config {
    bool allow_time_stretching;
    bool enable_rtx_handling;
    int max_packets_in_buffer;
    int base_min_delay_ms;
    std::unique_ptr<NetEqFacade> neteq_facade;
    TickTimer* tick_timer;
  };

  struct PacketInfo {
    uint32_t timestamp;
    bool is_dtx;
    bool is_cng;
  };

  NetEqController(const NetEqController&) = delete;
  NetEqController& operator=(const NetEqController&) = delete;
  virtual ~NetEqController();

  // Resets object to a clean state.
  virtual void Reset() = 0;

  // Resets parts of the state. Typically done when switching codecs.
  virtual void SoftReset() = 0;

  // Returns the operation that should be done next. |target_timestamp| and
  // |expand_mutefactor| are provided for reference. |decoder_frame_length| is
  // the number of samples obtained from the last decoded frame. If there is a
  // packet available, it should be supplied in |next_packet|; otherwise it
  // should be NULL. The mode resulting from the last call to
  // NetEqImpl::GetAudio is supplied in |prev_mode|. If there is a DTMF event to
  // play, |play_dtmf| should be set to true. The output variable
  // |reset_decoder| will be set to true if a reset is required; otherwise it is
  // left unchanged (i.e., it can remain true if it was true before the call).
  // This method end with calling GetDecisionSpecialized to get the actual
  // return value.
  virtual Operations GetDecision(uint32_t target_timestamp,
                                 int16_t expand_mutefactor,
                                 size_t decoder_frame_length,
                                 absl::optional<PacketInfo> next_packet,
                                 Modes prev_mode,
                                 bool play_dtmf,
                                 size_t generated_noise_samples,
                                 bool* reset_decoder) = 0;

  // Inform NetEqController that an empty packet has arrived.
  virtual void RegisterEmptyPacket() = 0;

  // Sets the sample rate and the output block size.
  virtual void SetSampleRate(int fs_hz, size_t output_size_samples) = 0;

  // Sets a minimum or maximum delay in millisecond.
  // Returns true if the delay bound is successfully applied, otherwise false.
  virtual bool SetMaximumDelay(int delay_ms) = 0;
  virtual bool SetMinimumDelay(int delay_ms) = 0;

  // Sets a base minimum delay in milliseconds for packet buffer. The effective
  // minimum delay can't be lower than base minimum delay, even if a lower value
  // is set using SetMinimumDelay.
  // Returns true if the base minimum is successfully applied, otherwise false.
  virtual bool SetBaseMinimumDelay(int delay_ms) = 0;
  virtual int GetBaseMinimumDelay() const = 0;

  // These methods test the |cng_state_| for different conditions.
  virtual bool CngRfc3389On() const = 0;
  virtual bool CngOff() const = 0;

  // Resets the |cng_state_| to kCngOff.
  virtual void SetCngOff() = 0;

  // Reports back to DecisionLogic whether the decision to do expand remains or
  // not. Note that this is necessary, since an expand decision can be changed
  // to kNormal in NetEqImpl::GetDecision if there is still enough data in the
  // sync buffer.
  virtual void ExpandDecision(Operations operation) = 0;

  // Adds |value| to |sample_memory_|.
  virtual void AddSampleMemory(int32_t value) = 0;

  // Returns the target buffer level in ms.
  virtual int TargetLevelMs() = 0;

  virtual void LastDecodedWasCngOrDtmf(bool last_cng_or_dtmf,
                                       size_t packet_length_samples,
                                       bool should_update_stats,
                                       uint16_t main_sequence_number,
                                       uint32_t main_timestamp,
                                       int fs_hz) = 0;
  // Returns true if a peak was found.
  virtual bool PeakFound() const = 0;

  // Accessors and mutators.
  virtual void set_sample_memory(int32_t value) = 0;
  virtual size_t noise_fast_forward() const = 0;
  virtual size_t packet_length_samples() const = 0;
  virtual void set_packet_length_samples(size_t value) = 0;
  virtual void set_prev_time_scale(bool value) = 0;

 protected:
  NetEqController();
};

class NetEqControllerFactory {
 public:
  virtual ~NetEqControllerFactory();
  virtual std::unique_ptr<NetEqController> Create(
      NetEqController::Config config) = 0;

 protected:
  NetEqControllerFactory();
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_NETEQ_CONTROLLER_H_
