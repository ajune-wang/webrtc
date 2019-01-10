/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_encoded_image_id_injector.h"

#include <cstddef>

#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {
// The amount on which encoded image buffer will be expanded to inject frame id.
// This is 2 bytes for uint16_t frame id itself and 2 bytes for original length
// of the buffer.
constexpr int kEncodedImageBufferExpansion = 4;
constexpr size_t kInitialBufferSize = 2 * 1024;
constexpr size_t kBuffersPoolSize = 256;
}  // namespace

DefaultEncodedImageIdInjector::DefaultEncodedImageIdInjector(int bufs_count) {
  for (int i = 0; i < bufs_count; i++) {
    bufs_pool_.push_back(std::vector<uint8_t>(kInitialBufferSize));
  }
  cur_buffer_ = 0;
}
DefaultEncodedImageIdInjector::~DefaultEncodedImageIdInjector() = default;

EncodedImage DefaultEncodedImageIdInjector::InjectId(
    uint16_t id,
    const EncodedImage& source) {
  EncodedImage out = source;
  std::vector<uint8_t>* buffer = NextBuffer();
  if (buffer->size() < source._length + kEncodedImageBufferExpansion) {
    buffer->resize(source._length + kEncodedImageBufferExpansion);
  }
  out.set_buffer(buffer->data(), buffer->size());
  out._length = source._length + kEncodedImageBufferExpansion;
  memcpy(&out._buffer[kEncodedImageBufferExpansion], source._buffer,
         source._length);
  out._buffer[0] = id & 0x00ff;
  out._buffer[1] = (id & 0xff00) >> 8;
  out._buffer[2] = source._length & 0x00ff;
  out._buffer[3] = (source._length & 0xff00) >> 8;
  return out;
}

std::pair<uint16_t, EncodedImage> DefaultEncodedImageIdInjector::ExtractId(
    const EncodedImage& source) {
  EncodedImage out = source;
  std::vector<uint8_t>* buffer = NextBuffer();
  if (buffer->size() < source.capacity() - kEncodedImageBufferExpansion) {
    buffer->resize(source.capacity() - kEncodedImageBufferExpansion);
  }
  out.set_buffer(buffer->data(), buffer->size());

  size_t source_pos = 0;
  size_t out_pos = 0;
  absl::optional<uint16_t> id = absl::nullopt;
  while (source_pos < source._length) {
    RTC_CHECK_LE(source_pos + kEncodedImageBufferExpansion, source._length);
    uint16_t next_id =
        source._buffer[source_pos] + (source._buffer[source_pos + 1] << 8);
    RTC_CHECK(!id || id.value() == next_id)
        << "Different frames encoded into single encoded image: " << id.value()
        << " vs " << next_id;
    id = next_id;
    uint16_t length =
        source._buffer[source_pos + 2] + (source._buffer[source_pos + 3] << 8);
    RTC_CHECK_LE(source_pos + kEncodedImageBufferExpansion + length,
                 source._length);
    memcpy(&out._buffer[out_pos],
           &source._buffer[source_pos + kEncodedImageBufferExpansion], length);
    source_pos += length + kEncodedImageBufferExpansion;
    out_pos += length;
  }
  out._length = out_pos;

  return std::pair<uint16_t, EncodedImage>(id.value(), out);
}

std::vector<uint8_t>* DefaultEncodedImageIdInjector::NextBuffer() {
  cur_buffer_ = (cur_buffer_ + 1) % bufs_pool_.size();
  return &bufs_pool_.at(cur_buffer_);
}

DefaultQualityAnalyzingVideoContext::DefaultQualityAnalyzingVideoContext() =
    default;
DefaultQualityAnalyzingVideoContext::~DefaultQualityAnalyzingVideoContext() =
    default;

EncodedImageIdInjector* DefaultQualityAnalyzingVideoContext::GetIdInjector(
    int coding_entity_id,
    CodingType type) {
  rtc::CritScope crit(&lock_);
  auto it = injectors_.find({coding_entity_id, type});
  if (it == injectors_.end()) {
    auto injector =
        absl::make_unique<DefaultEncodedImageIdInjector>(kBuffersPoolSize);
    EncodedImageIdInjector* out = injector.get();
    injectors_[{coding_entity_id, type}] = std::move(injector);
    return out;
  }
  return it->second.get();
}

}  // namespace test
}  // namespace webrtc
