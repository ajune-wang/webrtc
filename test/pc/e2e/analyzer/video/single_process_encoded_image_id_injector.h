/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_SINGLE_PROCESS_ENCODED_IMAGE_ID_INJECTOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_SINGLE_PROCESS_ENCODED_IMAGE_ID_INJECTOR_H_

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/video/encoded_image.h"
#include "rtc_base/criticalsection.h"
#include "test/pc/e2e/analyzer/video/encoded_image_id_injector.h"

namespace webrtc {
namespace test {

// Based on assumption that all call participants are in the same OS process
// and uses same QualityAnalyzingVideoContext to obtain EncodedImageIdInjector.
//
// To inject frame id into EncodedImage injector uses first 2 bytes of
// EncodedImage payload. Then it uses 3rd byte for frame sub id, that is
// required to distinguish different spatial layers. The origin data from these
// 3 bytes will be stored inside injector's internal storage and then will be
// restored during extraction phase.
//
// This injector won't add any extra overhead into EncodedImage payload and
// support frames with any size of payload. Also assumes that every EncodedImage
// payload size is greater or equals to 3 bytes
class SingleProcessEncodedImageIdInjector : public EncodedImageIdInjector {
 public:
  SingleProcessEncodedImageIdInjector();
  ~SingleProcessEncodedImageIdInjector() override;

  EncodedImage InjectId(uint16_t id,
                        const EncodedImage& source,
                        int coding_entity_id) override;
  std::pair<uint16_t, EncodedImage> ExtractId(const EncodedImage& source,
                                              int coding_entity_id) override;

 private:
  // Contains data required to extract frame id from EncodedImage and restore
  // original buffer.
  struct ExtractionInfo {
    // Frame sub id to distinguish encoded images for different spatial layers.
    uint8_t sub_id = 0;
    // Length of the origin buffer encoded image.
    size_t length;
    // Data from first 3 bytes of origin encoded image's payload.
    uint8_t origin_data[3];
  };

  struct ExtractionInfoVector {
    ExtractionInfoVector();
    ~ExtractionInfoVector();
    ExtractionInfoVector(ExtractionInfoVector const&);
    ExtractionInfoVector(ExtractionInfoVector&&);

    // Next sub id, that have to be used for this frame id.
    uint8_t next_sub_id = 0;
    std::vector<ExtractionInfo> infos;
  };

  rtc::CriticalSection lock_;
  std::map<uint16_t, ExtractionInfoVector> extraction_cache_
      RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_SINGLE_PROCESS_ENCODED_IMAGE_ID_INJECTOR_H_
