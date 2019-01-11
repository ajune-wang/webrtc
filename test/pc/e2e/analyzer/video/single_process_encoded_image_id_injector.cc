/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/single_process_encoded_image_id_injector.h"

#include <cstddef>

#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

SingleProcessEncodedImageIdInjector::SingleProcessEncodedImageIdInjector() =
    default;
SingleProcessEncodedImageIdInjector::~SingleProcessEncodedImageIdInjector() =
    default;

EncodedImage SingleProcessEncodedImageIdInjector::InjectId(
    uint16_t id,
    const EncodedImage& source,
    int coding_entity_id) {
  RTC_CHECK(source.size() >= 3);

  ExtractionInfo info;
  info.length = source.size();
  memcpy(info.origin_data, source.data(), 3);
  {
    rtc::CritScope crit(&lock_);
    // Will create new one if missed.
    ExtractionInfoVector& ev = extraction_cache_[id];
    info.sub_id = ev.next_sub_id++;
    ev.infos.push_back(info);
  }

  EncodedImage out = source;
  out.data()[0] = id & 0x00ff;
  out.data()[1] = (id & 0xff00) >> 8;
  out.data()[2] = info.sub_id;
  return out;
}

std::pair<uint16_t, EncodedImage>
SingleProcessEncodedImageIdInjector::ExtractId(const EncodedImage& source,
                                               int coding_entity_id) {
  EncodedImage out = source;

  size_t pos = 0;
  absl::optional<uint16_t> id = absl::nullopt;
  while (pos < source.size()) {
    // Extract frame id from first 2 bytes of the payload.
    uint16_t next_id = source.data()[pos] + (source.data()[pos + 1] << 8);
    // Extract frame sub id from second 2 byte of the payload.
    uint16_t sub_id = source.data()[pos + 2];

    RTC_CHECK(!id || id.value() == next_id)
        << "Different frames encoded into single encoded image: " << id.value()
        << " vs " << next_id;
    id = next_id;

    ExtractionInfo info;
    {
      rtc::CritScope crit(&lock_);
      auto it = extraction_cache_.find(next_id);
      RTC_CHECK(it != extraction_cache_.end())
          << "Unknown frame id " << next_id;

      bool found = false;
      for (size_t i = 0; i < it->second.infos.size(); i++) {
        if (it->second.infos[i].sub_id == sub_id) {
          info = it->second.infos[i];
          it->second.infos[i] = it->second.infos[it->second.infos.size() - 1];
          it->second.infos.pop_back();
          found = true;
        }
      }
      RTC_CHECK(found) << "Unknown sub id " << sub_id << " for frame "
                       << next_id;
    }

    memcpy(&out.data()[pos], info.origin_data, 3);
    pos += info.length;
  }
  out.set_size(pos);

  return std::pair<uint16_t, EncodedImage>(id.value(), out);
}

SingleProcessEncodedImageIdInjector::ExtractionInfoVector::
    ExtractionInfoVector() = default;
SingleProcessEncodedImageIdInjector::ExtractionInfoVector::
    ~ExtractionInfoVector() = default;
SingleProcessEncodedImageIdInjector::ExtractionInfoVector::ExtractionInfoVector(
    ExtractionInfoVector const&) = default;
SingleProcessEncodedImageIdInjector::ExtractionInfoVector::ExtractionInfoVector(
    ExtractionInfoVector&&) = default;

}  // namespace test
}  // namespace webrtc
