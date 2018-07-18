/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_COLUMN_PRINTER_H_
#define TEST_SCENARIO_COLUMN_PRINTER_H_
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {
class LambdaPrinter {
 public:
  LambdaPrinter(const char* headers,
                std::function<void(rtc::SimpleStringBuilder&)> printer,
                size_t max_length = 256);

 protected:
  friend class ColumnPrinter;
  const char* headers_;
  std::function<void(rtc::SimpleStringBuilder&)> printer_;
  size_t max_length_;
};

class ColumnPrinter {
 public:
  ColumnPrinter(std::string filename, std::vector<LambdaPrinter> printers);
  explicit ColumnPrinter(std::vector<LambdaPrinter> printers);
  ColumnPrinter(const ColumnPrinter&) = delete;
  ColumnPrinter& operator=(const ColumnPrinter&) = delete;
  ~ColumnPrinter();
  void PrintHeaders();
  void PrintRow();

 private:
  const std::vector<LambdaPrinter> printers_;
  size_t buffer_size_ = 0;
  std::vector<char> buffer_;
  FILE* output_file_ = nullptr;
  FILE* output_ = nullptr;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_COLUMN_PRINTER_H_
