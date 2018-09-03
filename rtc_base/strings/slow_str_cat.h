/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGS_SLOW_STR_CAT_H_
#define RTC_BASE_STRINGS_SLOW_STR_CAT_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/inline.h"

namespace rtc {

namespace slow_str_cat_impl {

enum class ArgType : int8_t {
  kEnd = 0,
  kCharP,
  kStringView,
  kStdString,
  kInt,
  kLong,
  kLongLong,
  kUInt,
  kULong,
  kULongLong,
  kDouble,
  kHex,
  kDec,
};

template <ArgType TypeNum, size_t BufSize, typename T>
struct ArgFormat {
  static constexpr ArgType Type() { return TypeNum; }
  static constexpr size_t BufferSize() { return BufSize; }
  T val;
};

// For each argument type accepted by rtc::SlowStrCat, there is a
// SlowStrCatFormat overload that takes one argument of that type and returns
// it wrapped in an ArgFormat, which has three members: `val` is the value that
// will be pushed on the stack when calling SlowStrCatImpl; `Type()` is an enum
// that uniquely identifies the type of `val`; and `BufferSize()` is the max
// number of characters of scratch space SlowStrCatImpl will need when
// processing this argument.

template <typename = void>
inline ArgFormat<ArgType::kCharP, 0, const char*> SlowStrCatFormat(
    const char* x) {
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kStringView, 0, const absl::string_view*>
SlowStrCatFormat(const absl::string_view& x) {
  return {&x};
}

template <typename = void>
inline ArgFormat<ArgType::kStdString, 0, const std::string*> SlowStrCatFormat(
    const std::string& x) {
  return {&x};
}

template <typename = void>
inline ArgFormat<ArgType::kInt, 11, int> SlowStrCatFormat(int x) {
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kLong, 20, long>  // NOLINT(runtime/int)
SlowStrCatFormat(long x) {                  // NOLINT(runtime/int)
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kLongLong, 20, long long>  // NOLINT(runtime/int)
SlowStrCatFormat(long long x) {                      // NOLINT(runtime/int)
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kUInt, 10, unsigned> SlowStrCatFormat(unsigned x) {
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kULong, 20, unsigned long>  // NOLINT(runtime/int)
SlowStrCatFormat(unsigned long x) {                   // NOLINT(runtime/int)
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kULongLong,
                 20,
                 unsigned long long>      // NOLINT(runtime/int)
SlowStrCatFormat(unsigned long long x) {  // NOLINT(runtime/int)
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kDouble, sizeof(absl::AlphaNum), double>
SlowStrCatFormat(double x) {
  return {x};
}

template <typename = void>
inline ArgFormat<ArgType::kHex, sizeof(absl::AlphaNum), const absl::Hex*>
SlowStrCatFormat(const absl::Hex& x) {
  return {&x};
}

template <typename = void>
inline ArgFormat<ArgType::kDec, sizeof(absl::AlphaNum), const absl::Dec*>
SlowStrCatFormat(const absl::Dec& x) {
  return {&x};
}

// This is just like std::underlying_type, except its `type` member is void if
// `T` isn't an enum. We need this below, since otherwise the compiler barfs on
// non-enum types even though SFINAE ought to mean it's supposed to just
// disregard them.
template <typename T, bool IsEnum = std::is_enum<T>{}>
struct UnderlyingType;

template <typename T>
struct UnderlyingType<T, true> {
  using type = typename std::underlying_type<T>::type;
};

template <typename T>
struct UnderlyingType<T, false> {
  using type = void;
};

// Normal enums are already handled by the integer SlowStrCatFormat overloads.
// This overload matches only scoped enums.
template <typename T,
          typename = typename std::enable_if<
              std::is_enum<T>{} && !std::is_convertible<T, int>{}>::type>
inline auto SlowStrCatFormat(T x) -> decltype(
    SlowStrCatFormat(static_cast<typename UnderlyingType<T>::type>(x))) {
  return SlowStrCatFormat(static_cast<typename UnderlyingType<T>::type>(x));
}

struct AllocateOnCallersStackFrame {
  explicit AllocateOnCallersStackFrame() {}  // NOLINT(runtime/explicit)
  absl::string_view sv;
};

// Everything not handled by the other SlowStrCatFormat overloads, we feed to
// absl::AlphaNum. The caller should *not* override the default value of the
// second argument.
template <typename T,
          typename = typename std::enable_if<
              !std::is_arithmetic<T>{} && !std::is_enum<T>{} &&
              !std::is_convertible<T, absl::string_view>{}>::type>
inline auto SlowStrCatFormat(
    const absl::AlphaNum& x,
    AllocateOnCallersStackFrame&& temp = AllocateOnCallersStackFrame())
    -> decltype(SlowStrCatFormat(temp.sv)) {
  temp.sv = x.Piece();
  // We end up returning a pointer to temp.sv here because
  // SlowStrCatFormat(const absl::string_view&) returns a pointer to its
  // argument. That's why we very carefully allocate it on the *caller's* stack
  // frame, where it will live until the caller is done evaluating the entire
  // expression containing the call.
  return SlowStrCatFormat(temp.sv);
}

constexpr size_t Sum() {
  return 0;
}

template <typename... Ts>
constexpr size_t Sum(size_t x, Ts... rest) {
  return x + Sum(rest...);
}

// Like string_view, but trivially default constructible.
struct ArgScratch {
  const char* data;
  size_t size;
};

// Non-inlined implementation. Called with a compiler-generated format string,
// caller-allocated scratch space, and a variable number of actual arguments.
std::string SlowStrCatImpl(const ArgType* fmt, ArgScratch* scratch, ...);

}  // namespace slow_str_cat_impl

// This is a drop-in alternative to absl::StrCat, which is slower but compiles
// to much less code at each call site.
template <typename... Ts>
RTC_FORCE_INLINE inline std::string SlowStrCat(const Ts&... args) {
  // Compiler-generated format string.
  static constexpr slow_str_cat_impl::ArgType fmt[] = {
      decltype(slow_str_cat_impl::SlowStrCatFormat<Ts>(args))::Type()...,
      slow_str_cat_impl::ArgType::kEnd};

  // How much scratch space do we need for all of the arguments combined?
  constexpr size_t kBufferSize = slow_str_cat_impl::Sum(
      decltype(slow_str_cat_impl::SlowStrCatFormat<Ts>(args))::BufferSize()...);

  // SlowStrCatImpl can't allocate scratch space itself, because C++ functions
  // can't allocate variable-sized arrays on the stack. But we can allocate it
  // here, because here the number of arguments and the sum of their scratch
  // space needs are constants.
  struct Scratch {
    slow_str_cat_impl::ArgScratch args[sizeof...(Ts)];
    char buf[kBufferSize];
  } scratch;
  static_assert(offsetof(Scratch, buf) ==
                    sizeof...(Ts) * sizeof(slow_str_cat_impl::ArgScratch),
                "");

  // Allocating the scratch space is very cheap: just make some extra room on
  // the stack. This takes approximately zero instructions.
  static_assert(std::is_trivial<Scratch>::value, "");

  // Call the non-inlined function that does all the work.
  return slow_str_cat_impl::SlowStrCatImpl(
      fmt, &scratch.args[0],
      slow_str_cat_impl::SlowStrCatFormat<Ts>(args).val...);
}

}  // namespace rtc

#endif  // RTC_BASE_STRINGS_SLOW_STR_CAT_H_
