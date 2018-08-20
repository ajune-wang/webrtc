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
class ColumnPrinter {
 public:
  ColumnPrinter(const ColumnPrinter&);
  ~ColumnPrinter();
  static ColumnPrinter Fixed(const char* headers, std::string fields);
  static ColumnPrinter Lambda(
      const char* headers,
      std::function<void(rtc::SimpleStringBuilder&)> printer,
      size_t max_length = 256);

 protected:
  friend class ColumnsPrinter;
  const char* headers_;
  std::function<void(rtc::SimpleStringBuilder&)> printer_;
  size_t max_length_;

 private:
  ColumnPrinter(const char* headers,
                std::function<void(rtc::SimpleStringBuilder&)> printer,
                size_t max_length);
};

class ColumnsPrinter {
 public:
  ColumnsPrinter(std::string filename, std::vector<ColumnPrinter> printers);
  explicit ColumnsPrinter(std::vector<ColumnPrinter> printers);
  ColumnsPrinter(const ColumnsPrinter&) = delete;
  ColumnsPrinter& operator=(const ColumnsPrinter&) = delete;
  ~ColumnsPrinter();
  void PrintHeaders();
  void PrintRow();

 private:
  const std::vector<ColumnPrinter> printers_;
  size_t buffer_size_ = 0;
  std::vector<char> buffer_;
  FILE* output_file_ = nullptr;
  FILE* output_ = nullptr;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_COLUMN_PRINTER_H_
