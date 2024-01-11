/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCStatisticsReport+Private.h"

#include "absl/types/variant.h"
#include "helpers/NSString+StdString.h"
#include "rtc_base/checks.h"

namespace webrtc {

/** Converts a single value to a suitable NSNumber, NSString or NSArray containing NSNumbers
    or NSStrings, or NSDictionary of NSString keys to NSNumber values.*/
NSObject *ValueFromStatsAttribute(const Attribute &attribute) {
  if (!attribute.has_value()) {
    return nil;
  }
  const auto &variant = attribute.as_variant();
  if (absl::holds_alternative<const RTCStatsMember<bool> *>(variant)) {
    return [NSNumber numberWithBool:absl::get<const RTCStatsMember<bool> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<int32_t> *>(variant)) {
    return [NSNumber numberWithInt:absl::get<const RTCStatsMember<int32_t> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<uint32_t> *>(variant)) {
    return [NSNumber
        numberWithUnsignedInt:absl::get<const RTCStatsMember<uint32_t> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<int64_t> *>(variant)) {
    return [NSNumber numberWithLong:absl::get<const RTCStatsMember<int64_t> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<uint64_t> *>(variant)) {
    return [NSNumber
        numberWithUnsignedLong:absl::get<const RTCStatsMember<uint64_t> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<double> *>(variant)) {
    return [NSNumber numberWithDouble:absl::get<const RTCStatsMember<double> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<std::string> *>(variant)) {
    return [NSString
        stringForStdString:absl::get<const RTCStatsMember<std::string> *>(variant)->value()];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<bool>> *>(variant)) {
    std::vector<bool> sequence =
        absl::get<const RTCStatsMember<std::vector<bool>> *>(variant)->value();
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (auto item : sequence) {
      [array addObject:[NSNumber numberWithBool:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<int32_t>> *>(variant)) {
    std::vector<int32_t> sequence =
        absl::get<const RTCStatsMember<std::vector<int32_t>> *>(variant)->value();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithInt:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<uint32_t>> *>(variant)) {
    std::vector<uint32_t> sequence =
        absl::get<const RTCStatsMember<std::vector<uint32_t>> *>(variant)->value();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithUnsignedInt:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<int64_t>> *>(variant)) {
    std::vector<int64_t> sequence =
        absl::get<const RTCStatsMember<std::vector<int64_t>> *>(variant)->value();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithLong:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<uint64_t>> *>(variant)) {
    std::vector<uint64_t> sequence =
        absl::get<const RTCStatsMember<std::vector<uint64_t>> *>(variant)->value();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithUnsignedLong:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<double>> *>(variant)) {
    std::vector<double> sequence =
        absl::get<const RTCStatsMember<std::vector<double>> *>(variant)->value();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithDouble:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::vector<std::string>> *>(variant)) {
    std::vector<std::string> sequence =
        absl::get<const RTCStatsMember<std::vector<std::string>> *>(variant)->value();
    NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSString stringForStdString:item]];
    }
    return [array copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::map<std::string, uint64_t>> *>(
                 variant)) {
    std::map<std::string, uint64_t> map =
        absl::get<const RTCStatsMember<std::map<std::string, uint64_t>> *>(variant)->value();
    NSMutableDictionary<NSString *, NSNumber *> *dictionary =
        [NSMutableDictionary dictionaryWithCapacity:map.size()];
    for (const auto &item : map) {
      dictionary[[NSString stringForStdString:item.first]] = @(item.second);
    }
    return [dictionary copy];
  } else if (absl::holds_alternative<const RTCStatsMember<std::map<std::string, double>> *>(
                 variant)) {
    std::map<std::string, double> map =
        absl::get<const RTCStatsMember<std::map<std::string, double>> *>(variant)->value();
    NSMutableDictionary<NSString *, NSNumber *> *dictionary =
        [NSMutableDictionary dictionaryWithCapacity:map.size()];
    for (const auto &item : map) {
      dictionary[[NSString stringForStdString:item.first]] = @(item.second);
    }
    return [dictionary copy];
  }
  RTC_DCHECK_NOTREACHED();
  return nil;
}
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCStatistics)

@synthesize id = _id;
@synthesize timestamp_us = _timestamp_us;
@synthesize type = _type;
@synthesize values = _values;

- (instancetype)initWithStatistics:(const webrtc::RTCStats &)statistics {
  if (self = [super init]) {
    _id = [NSString stringForStdString:statistics.id()];
    _timestamp_us = statistics.timestamp().us();
    _type = [NSString stringWithCString:statistics.type() encoding:NSUTF8StringEncoding];

    NSMutableDictionary<NSString *, NSObject *> *values = [NSMutableDictionary dictionary];
    for (const auto &attribute : statistics.Attributes()) {
      NSObject *value = ValueFromStatsAttribute(attribute);
      if (value) {
        NSString *name = [NSString stringWithCString:attribute.name()
                                            encoding:NSUTF8StringEncoding];
        RTC_DCHECK(name.length > 0);
        RTC_DCHECK(!values[name]);
        values[name] = value;
      }
    }
    _values = [values copy];
  }

  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"id = %@, type = %@, timestamp = %.0f, values = %@",
                                    self.id,
                                    self.type,
                                    self.timestamp_us,
                                    self.values];
}

@end

@implementation RTC_OBJC_TYPE (RTCStatisticsReport)

@synthesize timestamp_us = _timestamp_us;
@synthesize statistics = _statistics;

- (NSString *)description {
  return [NSString
      stringWithFormat:@"timestamp = %.0f, statistics = %@", self.timestamp_us, self.statistics];
}

@end

@implementation RTC_OBJC_TYPE (RTCStatisticsReport) (Private)

- (instancetype)initWithReport : (const webrtc::RTCStatsReport &)report {
  if (self = [super init]) {
    _timestamp_us = report.timestamp().us();

    NSMutableDictionary *statisticsById =
        [NSMutableDictionary dictionaryWithCapacity:report.size()];
    for (const auto &stat : report) {
      RTC_OBJC_TYPE(RTCStatistics) *statistics =
          [[RTC_OBJC_TYPE(RTCStatistics) alloc] initWithStatistics:stat];
      statisticsById[statistics.id] = statistics;
    }
    _statistics = [statisticsById copy];
  }

  return self;
}

@end
