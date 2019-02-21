/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection+Private.h"

#import "RTCLegacyStatsReport+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCStatisticsReport+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace webrtc {

class StatsCollectorCallbackAdaptor : public RTCStatsCollectorCallback {
 public:
  StatsCollectorCallbackAdaptor(void (^completionHandler)(RTCStatisticsReport *)) {
    completion_handler_ = completionHandler;
  }

  ~StatsCollectorCallbackAdaptor() override { completion_handler_ = nil; }

  void OnStatsDelivered(const rtc::scoped_refptr<const RTCStatsReport> &report) override {
    RTC_DCHECK(completion_handler_);
    RTCStatisticsReport *statisticsReport = [[RTCStatisticsReport alloc] initWithReport:*report];
    completion_handler_(statisticsReport);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)(RTCStatisticsReport *);
};

class StatsObserverAdapter : public StatsObserver {
 public:
  StatsObserverAdapter(void (^completionHandler)
      (NSArray<RTCLegacyStatsReport *> *stats)) {
    completion_handler_ = completionHandler;
  }

  ~StatsObserverAdapter() override { completion_handler_ = nil; }

  void OnComplete(const StatsReports& reports) override {
    RTC_DCHECK(completion_handler_);
    NSMutableArray *stats = [NSMutableArray arrayWithCapacity:reports.size()];
    for (const auto* report : reports) {
      RTCLegacyStatsReport *statsReport =
          [[RTCLegacyStatsReport alloc] initWithNativeReport:*report];
      [stats addObject:statsReport];
    }
    completion_handler_(stats);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)(NSArray<RTCLegacyStatsReport *> *stats);
};
}  // namespace webrtc

@implementation RTCPeerConnection (Stats)

- (void)statisticsWithCompletionHandler:(void (^)(RTCStatisticsReport *))completionHandler {
  rtc::scoped_refptr<webrtc::StatsCollectorCallbackAdaptor> collector(
      new rtc::RefCountedObject<webrtc::StatsCollectorCallbackAdaptor>(completionHandler));
  self.nativePeerConnection->GetStats(collector);
}

- (void)statsForTrack:(RTCMediaStreamTrack *)mediaStreamTrack
     statsOutputLevel:(RTCStatsOutputLevel)statsOutputLevel
    completionHandler:
    (void (^)(NSArray<RTCLegacyStatsReport *> *stats))completionHandler {
  rtc::scoped_refptr<webrtc::StatsObserverAdapter> observer(
      new rtc::RefCountedObject<webrtc::StatsObserverAdapter>
          (completionHandler));
  webrtc::PeerConnectionInterface::StatsOutputLevel nativeOutputLevel =
      [[self class] nativeStatsOutputLevelForLevel:statsOutputLevel];
  self.nativePeerConnection->GetStats(
      observer, mediaStreamTrack.nativeTrack, nativeOutputLevel);
}

@end
