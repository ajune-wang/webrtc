/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/column_printer.h"

namespace webrtc {
namespace test {

ColumnPrinter::ColumnPrinter(const ColumnPrinter&) = default;
ColumnPrinter::~ColumnPrinter() = default;

ColumnPrinter::ColumnPrinter(
    const char* headers,
    std::function<void(rtc::SimpleStringBuilder&)> printer,
    size_t max_length)
    : headers_(headers), printer_(printer), max_length_(max_length) {}

ColumnPrinter ColumnPrinter::Fixed(const char* headers, std::string fields) {
  return ColumnPrinter(headers,
                       [fields](rtc::SimpleStringBuilder& sb) { sb << fields; },
                       fields.size());
}

ColumnPrinter ColumnPrinter::Lambda(
    const char* headers,
    std::function<void(rtc::SimpleStringBuilder&)> printer,
    size_t max_length) {
  return ColumnPrinter(headers, printer, max_length);
}

ColumnsPrinter::ColumnsPrinter(std::string filename,
                               std::vector<ColumnPrinter> printers)
    : ColumnsPrinter(printers) {
  output_file_ = fopen(filename.c_str(), "w");
  RTC_CHECK(output_file_);
  output_ = output_file_;
}

ColumnsPrinter::ColumnsPrinter(std::vector<ColumnPrinter> printers)
    : printers_(printers) {
  output_ = stdout;
  RTC_CHECK(!printers_.empty());
  for (auto& printer : printers_)
    buffer_size_ += printer.max_length_ + 1;
  buffer_.resize(buffer_size_);
}

ColumnsPrinter::~ColumnsPrinter() {
  if (output_file_)
    fclose(output_file_);
}

void ColumnsPrinter::PrintHeaders() {
  fprintf(output_, "%s", printers_[0].headers_);
  for (size_t i = 1; i < printers_.size(); ++i) {
    fprintf(output_, " %s", printers_[i].headers_);
  }
  fputs("\n", output_);
}

void ColumnsPrinter::PrintRow() {
  rtc::SimpleStringBuilder sb(buffer_);
  printers_[0].printer_(sb);
  for (size_t i = 1; i < printers_.size(); ++i) {
    sb << ' ';
    printers_[i].printer_(sb);
  }
  sb << "\n\0";
  fputs(&buffer_.front(), output_);
}
}  // namespace test
}  // namespace webrtc
