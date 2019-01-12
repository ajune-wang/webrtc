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

#include <utility>

#include "api/video/encoded_image.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
uint8_t* CreateBufferOfSizeNFilledWithValuesFromX(size_t n, uint8_t x) {
  uint8_t* buffer = new uint8_t[n];
  for (size_t i = 0; i < n; i++) {
    buffer[i] = x + i;
  }
  return buffer;
}
}  // namespace

TEST(SingleProcessEncodedImageIdInjector, InjectExtract) {
  SingleProcessEncodedImageIdInjector injector;

  uint8_t* buffer = CreateBufferOfSizeNFilledWithValuesFromX(10, 1);

  EncodedImage source(buffer, 10, 10);
  source.SetTimestamp(123456789);

  std::pair<uint16_t, EncodedImage> out =
      injector.ExtractId(injector.InjectId(512, source, 1), 2);
  ASSERT_EQ(out.first, 512);
  ASSERT_EQ(out.second._length, 10ul);
  ASSERT_EQ(out.second.capacity(), 10ul);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(out.second._buffer[i], i + 1);
  }

  delete[] buffer;
}

TEST(SingleProcessEncodedImageIdInjector, Inject3Extract3) {
  SingleProcessEncodedImageIdInjector injector;

  uint8_t* buffer1 = CreateBufferOfSizeNFilledWithValuesFromX(10, 1);
  uint8_t* buffer2 = CreateBufferOfSizeNFilledWithValuesFromX(10, 11);
  uint8_t* buffer3 = CreateBufferOfSizeNFilledWithValuesFromX(10, 21);

  // 1st frame
  EncodedImage source1(buffer1, 10, 10);
  source1.SetTimestamp(123456710);
  // 2nd frame 1st spatial layer
  EncodedImage source2(buffer2, 10, 10);
  source2.SetTimestamp(123456720);
  // 2nd frame 2nd spatial layer
  EncodedImage source3(buffer3, 10, 10);
  source3.SetTimestamp(123456720);

  EncodedImage intermediate1 = injector.InjectId(510, source1, 1);
  EncodedImage intermediate2 = injector.InjectId(520, source2, 1);
  EncodedImage intermediate3 = injector.InjectId(520, source3, 1);

  // Extract ids in different order.
  std::pair<uint16_t, EncodedImage> out3 = injector.ExtractId(intermediate3, 2);
  std::pair<uint16_t, EncodedImage> out1 = injector.ExtractId(intermediate1, 2);
  std::pair<uint16_t, EncodedImage> out2 = injector.ExtractId(intermediate2, 2);

  ASSERT_EQ(out1.first, 510);
  ASSERT_EQ(out1.second._length, 10ul);
  ASSERT_EQ(out1.second.capacity(), 10ul);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(out1.second._buffer[i], i + 1);
  }
  ASSERT_EQ(out2.first, 520);
  ASSERT_EQ(out2.second._length, 10ul);
  ASSERT_EQ(out2.second.capacity(), 10ul);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(out2.second._buffer[i], i + 11);
  }
  ASSERT_EQ(out3.first, 520);
  ASSERT_EQ(out3.second._length, 10ul);
  ASSERT_EQ(out3.second.capacity(), 10ul);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(out3.second._buffer[i], i + 21);
  }

  delete[] buffer1;
  delete[] buffer2;
  delete[] buffer3;
}

TEST(SingleProcessEncodedImageIdInjector, InjectExtractFromConcatenated) {
  SingleProcessEncodedImageIdInjector injector;

  uint8_t* buffer1 = CreateBufferOfSizeNFilledWithValuesFromX(10, 1);
  uint8_t* buffer2 = CreateBufferOfSizeNFilledWithValuesFromX(10, 11);
  uint8_t* buffer3 = CreateBufferOfSizeNFilledWithValuesFromX(10, 21);

  EncodedImage source1(buffer1, 10, 10);
  source1.SetTimestamp(123456710);
  EncodedImage source2(buffer2, 10, 10);
  source2.SetTimestamp(123456710);
  EncodedImage source3(buffer3, 10, 10);
  source3.SetTimestamp(123456710);

  // Inject id into 3 images with same frame id.
  EncodedImage intermediate1 = injector.InjectId(512, source1, 1);
  EncodedImage intermediate2 = injector.InjectId(512, source2, 1);
  EncodedImage intermediate3 = injector.InjectId(512, source3, 1);

  // Concatenate them into single encoded image, like it can be done in jitter
  // buffer.
  size_t concatenated_length =
      intermediate1._length + intermediate2._length + intermediate3._length;
  uint8_t* concatenated_buffer = new uint8_t[concatenated_length];
  memcpy(concatenated_buffer, intermediate1._buffer, intermediate1._length);
  memcpy(&concatenated_buffer[intermediate1._length], intermediate2._buffer,
         intermediate2._length);
  memcpy(&concatenated_buffer[intermediate1._length + intermediate2._length],
         intermediate3._buffer, intermediate3._length);
  EncodedImage concatenated(concatenated_buffer, concatenated_length,
                            concatenated_length);

  // Extract frame id from concatenated image
  std::pair<uint16_t, EncodedImage> out = injector.ExtractId(concatenated, 2);

  ASSERT_EQ(out.first, 512);
  ASSERT_EQ(out.second._length, 3 * 10ul);
  ASSERT_EQ(out.second.capacity(), 3 * 10ul);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(out.second._buffer[i], i + 1);
    ASSERT_EQ(out.second._buffer[i + 10], i + 11);
    ASSERT_EQ(out.second._buffer[i + 20], i + 21);
  }

  delete[] buffer1;
  delete[] buffer2;
  delete[] buffer3;
  delete[] concatenated_buffer;
}

}  // namespace test
}  // namespace webrtc
