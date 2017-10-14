/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/null_audio_poller.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace internal {

constexpr int64_t kPollDelay = 10;  // 10ms

constexpr size_t kNumChannels = 1;
constexpr uint32_t kSamplesPerSecond = 44000;            // 44kHz
constexpr size_t kNumSamples = kSamplesPerSecond / 100;  // 10ms of samples

NullAudioPoller::NullAudioPoller(AudioTransport* audio_transport)
    : audio_transport_(audio_transport),
      reschedule_at_(rtc::TimeMillis() + kPollDelay) {
  RTC_DCHECK(audio_transport);
  OnMessage(nullptr);  // Start the poll loop.
}

NullAudioPoller::~NullAudioPoller() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rtc::Thread::Current()->Clear(this);
}

void NullAudioPoller::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "\n\nOne iteration of the loop\n\n";

  // Buffer to hold the audio samples.
  int16_t buffer[kNumSamples * kNumChannels];
  // Output variables from |NeedMorePlayData|.
  size_t n_samples;
  int64_t elapsed_time_ms;
  int64_t ntp_time_ms;
  audio_transport_->NeedMorePlayData(kNumSamples, sizeof(int16_t), kNumChannels,
                                     kSamplesPerSecond, buffer, n_samples,
                                     &elapsed_time_ms, &ntp_time_ms);

  // Reschedule the next poll iteration. If, for some reason, the given
  // reschedule time has already passed, reschedule as soon as possible.
  int64_t now = rtc::TimeMillis();
  if (reschedule_at_ < now) {
    reschedule_at_ = now;
  }
  rtc::Thread::Current()->PostAt(RTC_FROM_HERE, reschedule_at_, this, 0);

  // Loop after next will be kPollDelay milliseconds later.
  reschedule_at_ += kPollDelay;
}

}  // namespace internal
}  // namespace webrtc
