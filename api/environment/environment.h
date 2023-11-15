/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This header file provides wrapper for common webrtc primitives.
// Different application may need different implementations of these primitives,
// Moreover, single application may need to use webrtc for multiple purposes,
// and thus would need to provide different primitives implementations for
// different peer connections.
// The main purpose of the `Environment` class below is to propagate references
// to those primitives to all webrtc classes that need them.

#ifndef API_ENVIRONMENT_ENVIRONMENT_H_
#define API_ENVIRONMENT_ENVIRONMENT_H_

#include <utility>

#include "absl/base/nullability.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// These classes are forward declared to keep Environment dependencies
// lightweight. User that needs any of the types below should include their
// header explicitely.
class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

// Contains references to WebRTC primitives. Object of this class should be
// passed as a construction parameter and saved by value in each class that
// needs it. Most classes shouldn't create a new instance of the `Environment`,
// but instead should use a propagated copy.
// Example:
//    class PeerConnection {
//     public:
//      PeerConnection(const Environment& env, ...)
//          : env_(env),
//            rtp_manager_(env, ...),
//            ...
//
//      const FieldTrialsView& trials() const { return env_.field_trials(); }
//
//      scoped_refptr<RtpTransceiverInterface> AddTransceiver(...) {
//        return make_ref_counted<RtpTransceiverImpl>(env_, ...);
//      }
//
//     private:
//      const Environment env_;
//      RtpTransmissionManager rtp_manager_;
//    };
// This class is thread safe.
class RTC_EXPORT Environment final {
 public:
  // Default constructor is deleted in favor of creating this object using
  // `EnvironmentFactory`. To create a default environment use
  // `EnvironmentFactory().Create()`.
  Environment() = delete;

  Environment(const Environment&) = default;
  Environment(Environment&&) = default;
  Environment& operator=(const Environment&) = default;
  Environment& operator=(Environment&&) = default;

  ~Environment() = default;

  // Provides means to alter behavior, mostly for A/B testing new features.
  // See g3doc/field-trials.md
  const FieldTrialsView& field_trials() const;

  // Provides interface to query current time.
  // See g3doc/implementation_basics.md#time
  Clock& clock() const;

  // Provides multi tasking synchronization primitive.
  // See g3doc/implementation_basics.md#synchronization-primitives
  TaskQueueFactory& task_queue_factory() const;

  // Provides interface to collect structured logging.
  // See logging/g3doc/rtc_event_log.md
  RtcEventLog& event_log() const;

 private:
  friend class EnvironmentFactory;
  Environment(scoped_refptr<const rtc::RefCountedBase> storage,
              absl::Nonnull<const FieldTrialsView*> field_trials,
              absl::Nonnull<Clock*> clock,
              absl::Nonnull<TaskQueueFactory*> task_queue_factory,
              absl::Nonnull<RtcEventLog*> event_log)
      : storage_(std::move(storage)),
        field_trials_(field_trials),
        clock_(clock),
        task_queue_factory_(task_queue_factory),
        event_log_(event_log) {}

  // Container that keeps ownership of the primitives below.
  // Pointers below assumed to be valid while object in the `storage_` is alive.
  scoped_refptr<const rtc::RefCountedBase> storage_;

  absl::Nonnull<const FieldTrialsView*> field_trials_;
  absl::Nonnull<Clock*> clock_;
  absl::Nonnull<TaskQueueFactory*> task_queue_factory_;
  absl::Nonnull<RtcEventLog*> event_log_;
};

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

inline const FieldTrialsView& Environment::field_trials() const {
  return *field_trials_;
}

inline Clock& Environment::clock() const {
  return *clock_;
}

inline TaskQueueFactory& Environment::task_queue_factory() const {
  return *task_queue_factory_;
}

inline RtcEventLog& Environment::event_log() const {
  return *event_log_;
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_H_
