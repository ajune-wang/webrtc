/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRYPTSTRING_H_
#define RTC_BASE_CRYPTSTRING_H_

#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/buffer.h"

namespace rtc {

class CryptStringImpl {
 public:
  virtual ~CryptStringImpl() {}
  virtual size_t GetLength() const = 0;
  virtual void CopyTo(char * dest, bool nullterminate) const = 0;
  virtual std::string UrlEncode() const = 0;
  virtual CryptStringImpl * Copy() const = 0;
  virtual void CopyRawTo(std::vector<unsigned char> * dest) const = 0;
};

class EmptyCryptStringImpl : public CryptStringImpl {
 public:
  ~EmptyCryptStringImpl() override {}
  size_t GetLength() const override;
  void CopyTo(char* dest, bool nullterminate) const override;
  std::string UrlEncode() const override;
  CryptStringImpl* Copy() const override;
  void CopyRawTo(std::vector<unsigned char>* dest) const override;
};

class CryptString {
 public:
  CryptString();
  size_t GetLength() const { return impl_->GetLength(); }
  void CopyTo(char* dest, bool nullterminate) const {
    impl_->CopyTo(dest, nullterminate);
  }
  CryptString(const CryptString& other);
  explicit CryptString(const CryptStringImpl& impl);
  ~CryptString();
  CryptString & operator=(const CryptString & other) {
    if (this != &other) {
      impl_.reset(other.impl_->Copy());
    }
    return *this;
  }
  void Clear() { impl_.reset(new EmptyCryptStringImpl()); }
  std::string UrlEncode() const { return impl_->UrlEncode(); }
  void CopyRawTo(std::vector<unsigned char> * dest) const {
    return impl_->CopyRawTo(dest);
  }

 private:
  std::unique_ptr<const CryptStringImpl> impl_;
};

// Used for constructing strings where a password is involved and we
// need to ensure that we zero memory afterwards
class FormatCryptString {
 public:
  FormatCryptString() : data_(0, kInitialCapacity) { *data_.begin() = '\0'; }

  void Append(const std::string& text) { Append(text.data(), text.length()); }

  void Append(const char* data, size_t length) {
    data_.AppendData(length + 1, [data, length](ArrayView<char> dest) {
      memcpy(dest.data(), data, length);
      dest[length] = 0;
      return length + 1;
    });
  }

  void Append(const CryptString* password) {
    size_t len = password->GetLength();
    // The internal data is null terminated, reserve one additional byte.
    data_.EnsureCapacity(data_.size() + len + 1);
    password->CopyTo(data_.end(), true);
    data_.SetSize(data_.size() + len);
  }

  size_t GetLength() const { return data_.size(); }

  const char* GetData() const { return data_.data(); }

  // Ensures storage of at least n bytes
  void EnsureStorage(size_t n) { data_.EnsureCapacity(n); }

 private:
  static constexpr size_t kInitialCapacity = 32;

  ZeroOnFreeBuffer<char> data_;
};

class InsecureCryptStringImpl : public CryptStringImpl {
 public:
  std::string& password() { return password_; }
  const std::string& password() const { return password_; }

  ~InsecureCryptStringImpl() override = default;
  size_t GetLength() const override;
  void CopyTo(char* dest, bool nullterminate) const override;
  std::string UrlEncode() const override;
  CryptStringImpl* Copy() const override;
  void CopyRawTo(std::vector<unsigned char>* dest) const override;

 private:
  std::string password_;
};

}  // namespace rtc

#endif  // RTC_BASE_CRYPTSTRING_H_
