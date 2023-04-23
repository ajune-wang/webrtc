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
#include <utility>

#include "rtc_base/trace_event.h"

namespace webrtc {

constexpr TimeDelta SourceTracker::kTimeout;

SourceTracker::SourceTracker(Clock* clock)
    : worker_thread_(TaskQueueBase::Current()), clock_(clock) {
  RTC_DCHECK(worker_thread_);
}

void SourceTracker::OnFrameDelivered(const RtpPacketInfos& packet_infos) {
  if (packet_infos.empty()) {
    return;
  }

  // `RtpPacketInfos` holds a reference counted pointer to a vector in order
  // to avoid making copies of the data itself. By posting `packet_infos` we
  // `AddRef()` that buffer rather than copying.
  worker_thread_->PostTask(SafeTask(
      worker_safety_.flag(),
      [this, packet_infos = packet_infos, now = clock_->CurrentTime()]() {
        RTC_DCHECK_RUN_ON(worker_thread_);
        OnFrameDeliveredInternal(now, packet_infos);
      }));
}

void SourceTracker::OnFrameDeliveredInternal(
    Timestamp now,
    const RtpPacketInfos& packet_infos) {
  TRACE_EVENT0("webrtc", "SourceTracker::OnFrameDelivered");

  for (const RtpPacketInfo& packet_info : packet_infos) {
    for (uint32_t csrc : packet_info.csrcs()) {
      SourceKey key(RtpSourceType::CSRC, csrc);
      SourceEntry& entry = UpdateEntry(key);

      entry.timestamp = now;
      entry.audio_level = packet_info.audio_level();
      entry.absolute_capture_time = packet_info.absolute_capture_time();
      entry.local_capture_clock_offset =
          packet_info.local_capture_clock_offset();
      entry.rtp_timestamp = packet_info.rtp_timestamp();
    }

    SourceKey key(RtpSourceType::SSRC, packet_info.ssrc());
    SourceEntry& entry = UpdateEntry(key);

    entry.timestamp = now;
    entry.audio_level = packet_info.audio_level();
    entry.absolute_capture_time = packet_info.absolute_capture_time();
    entry.local_capture_clock_offset = packet_info.local_capture_clock_offset();
    entry.rtp_timestamp = packet_info.rtp_timestamp();

    FireAudioLevelCallback(key, entry.timestamp, entry.audio_level);
  }

  PruneEntries(now);
}

std::vector<RtpSource> SourceTracker::GetSources() const {
  RTC_DCHECK_RUN_ON(worker_thread_);

  Timestamp now = clock_->CurrentTime();
  PruneEntries(now);

  std::vector<RtpSource> sources;
  for (const auto& pair : list_) {
    const SourceKey& key = pair.first;
    const SourceEntry& entry = pair.second;

    sources.emplace_back(
        entry.timestamp.ms(), key.source, key.source_type, entry.rtp_timestamp,
        RtpSource::Extensions{
            .audio_level = entry.audio_level,
            .absolute_capture_time = entry.absolute_capture_time,
            .local_capture_clock_offset = entry.local_capture_clock_offset});
  }

  return sources;
}

absl::optional<uint8_t> SourceTracker::GetAudioLevel(uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(worker_thread_);

  Timestamp now = clock_->CurrentTime();
  PruneEntries(now);

  SourceKey key(RtpSourceType::SSRC, ssrc);
  auto map_it = map_.find(key);
  if (map_it == map_.end())
    return absl::nullopt;
  SourceList::iterator pair = map_it->second;
  const SourceEntry& entry = pair->second;
  return entry.audio_level;
}

void SourceTracker::SetAudioLevelCallback(
    uint32_t ssrc,
    absl::AnyInvocable<void(Timestamp, absl::optional<uint8_t>)>
        level_callback) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  SourceKey key(RtpSourceType::SSRC, ssrc);
  if (level_callback) {
    RTC_DCHECK(level_callbacks_.find(key) == level_callbacks_.end());
    level_callbacks_.emplace(key, std::move(level_callback));
  } else {
    // TODO(tommi): Figure out why Stop() is being called from
    // `AudioRtpReceiver::SetMediaChannel` without a matching Start().
    // See test RollbackRestoresFiredDirectionAndOnTrackCanFireAgain.
    // RTC_DCHECK(level_callbacks_.find(key) != level_callbacks_.end());
    level_callbacks_.erase(key);
  }
}

SourceTracker::SourceEntry& SourceTracker::UpdateEntry(const SourceKey& key) {
  // We intentionally do |find() + emplace()|, instead of checking the return
  // value of `emplace()`, for performance reasons. It's much more likely for
  // the key to already exist than for it not to.
  auto map_it = map_.find(key);
  if (map_it == map_.end()) {
    // Insert a new entry at the front of the list.
    list_.emplace_front(key, SourceEntry());
    map_.emplace(key, list_.begin());
  } else if (map_it->second != list_.begin()) {
    // Move the old entry to the front of the list.
    list_.splice(list_.begin(), list_, map_it->second);
  }

  return list_.front().second;
}

void SourceTracker::PruneEntries(Timestamp now) const {
  Timestamp prune = now - kTimeout;
  while (!list_.empty() && list_.back().second.timestamp < prune) {
    const auto& key = list_.back().first;
    if (key.source_type == RtpSourceType::SSRC)
      FireAudioLevelCallback(key, list_.back().second.timestamp, absl::nullopt);

    map_.erase(key);
    list_.pop_back();
  }
}

void SourceTracker::FireAudioLevelCallback(
    const SourceKey& key,
    Timestamp timestamp,
    absl::optional<uint8_t> level) const {
  auto callback = level_callbacks_.find(key);
  if (callback != level_callbacks_.end())
    callback->second(timestamp, level);
}

}  // namespace webrtc
