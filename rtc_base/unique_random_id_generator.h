/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_UNIQUE_RANDOM_ID_GENERATOR_H_
#define RTC_BASE_UNIQUE_RANDOM_ID_GENERATOR_H_

#include <limits>
#include <set>
#include <string>

#include "api/array_view.h"
#include "rtc_base/unique_number_generator.h"

namespace rtc {

// This class will generate unique ids. Ids are 32 bit unsigned integers.
// The generated ids will be unique, in the local scope of the generator.
// This means that a generator will never generate the same id twice.
// The generator can also be initialized with a sequence of known ids.
// In such a case, it will never generate an id from that list.
class UniqueRandomIdGenerator {
 public:
  typedef uint32_t value_type;
  UniqueRandomIdGenerator();
  // Create a generator that will never return any value from the given list.
  explicit UniqueRandomIdGenerator(ArrayView<uint32_t> known_ids);
  ~UniqueRandomIdGenerator();

  // Generates a random id that this generator has never produced before.
  // This method becomes more expensive with each use, as the probability of
  // collision for the randomly generated numbers increases.
  uint32_t GenerateId();
  uint32_t operator()() { return GenerateId(); }

  // Adds an id that this generator should no longer generate.
  // Return value indicates whether the ID was hitherto unknown.
  bool AddKnownId(uint32_t value);

 private:
  std::set<uint32_t> known_ids_;
};

// This class will generate strings. A common use case is for identifiers.
// The generated strings will be unique, in the local scope of the generator.
// This means that a generator will never generate the same string twice.
// The generator can also be initialized with a sequence of known ids.
// In such a case, it will never generate an id from that list.
class UniqueStringGenerator {
 public:
  typedef std::string value_type;
  UniqueStringGenerator();
  explicit UniqueStringGenerator(ArrayView<std::string> known_ids);
  ~UniqueStringGenerator();

  std::string GenerateString();
  std::string operator()() { return GenerateString(); }

  // Adds an id that this generator should no longer generate.
  // Return value indicates whether the ID was hitherto unknown.
  bool AddKnownId(const std::string& value);

 private:
  // This implementation will be simple and will generate "0", "1", ...
  UniqueNumberGenerator<uint32_t> unique_number_generator_;
};

}  // namespace rtc

#endif  // RTC_BASE_UNIQUE_RANDOM_ID_GENERATOR_H_
