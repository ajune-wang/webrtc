/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/environment/environment.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Ref;

class FakeEvent : public RtcEvent {
 public:
  Type GetType() const override { return RtcEvent::Type::FakeEvent; }
  bool IsConfigEvent() const override { return false; }
};

class FakeFieldTrials : public FieldTrialsView {
 public:
  explicit FakeFieldTrials(absl::AnyInvocable<void() &&> on_destroyed = nullptr)
      : on_destroyed_(std::move(on_destroyed)) {}
  ~FakeFieldTrials() override {
    if (on_destroyed_ != nullptr) {
      std::move(on_destroyed_)();
    }
  }

  std::string Lookup(absl::string_view key) const override { return "fake"; }

 private:
  absl::AnyInvocable<void() &&> on_destroyed_;
};

class FakeTaskQueueFactory : public TaskQueueFactory {
 public:
  explicit FakeTaskQueueFactory(
      absl::AnyInvocable<void() &&> on_destroyed = nullptr)
      : on_destroyed_(std::move(on_destroyed)) {}
  ~FakeTaskQueueFactory() override {
    if (on_destroyed_ != nullptr) {
      std::move(on_destroyed_)();
    }
  }

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return nullptr;
  }

 private:
  absl::AnyInvocable<void() &&> on_destroyed_;
};

TEST(EnvironmentTest, DefaultEnvironmentHasAllUtilities) {
  Environment env = EnvironmentFactory().Create();

  // Try to use each utility, expect no crashes.
  env.clock().CurrentTime();
  EXPECT_THAT(env.task_queue_factory().CreateTaskQueue(
                  "test", TaskQueueFactory::Priority::NORMAL),
              NotNull());
  env.event_log().Log(std::make_unique<FakeEvent>());
  env.field_trials().Lookup("WebRTC-Debugging-RtpDump");
}

TEST(EnvironmentTest, UsesProvidedUtilitiesWithOwnership) {
  std::unique_ptr<FieldTrialsView> owned_field_trials =
      std::make_unique<FakeFieldTrials>();
  std::unique_ptr<TaskQueueFactory> owned_task_queue_factory =
      std::make_unique<FakeTaskQueueFactory>();
  std::unique_ptr<Clock> owned_clock =
      std::make_unique<SimulatedClock>(Timestamp::Zero());
  std::unique_ptr<RtcEventLog> owned_event_log =
      std::make_unique<RtcEventLogNull>();

  FieldTrialsView& field_trials = *owned_field_trials;
  TaskQueueFactory& task_queue_factory = *owned_task_queue_factory;
  Clock& clock = *owned_clock;
  RtcEventLog& event_log = *owned_event_log;

  Environment env = EnvironmentFactory()
                        .With(std::move(owned_field_trials))
                        .With(std::move(owned_clock))
                        .With(std::move(owned_task_queue_factory))
                        .With(std::move(owned_event_log))
                        .Create();

  EXPECT_THAT(env.field_trials(), Ref(field_trials));
  EXPECT_THAT(env.task_queue_factory(), Ref(task_queue_factory));
  EXPECT_THAT(env.clock(), Ref(clock));
  EXPECT_THAT(env.event_log(), Ref(event_log));
}

TEST(EnvironmentTest, UsesProvidedUtilitiesWithoutOwnership) {
  FakeFieldTrials field_trials;
  FakeTaskQueueFactory task_queue_factory;
  SimulatedClock clock(Timestamp::Zero());
  RtcEventLogNull event_log;

  Environment env = EnvironmentFactory()
                        .With(&field_trials)
                        .With(&clock)
                        .With(&task_queue_factory)
                        .With(&event_log)
                        .Create();

  EXPECT_THAT(env.field_trials(), Ref(field_trials));
  EXPECT_THAT(env.task_queue_factory(), Ref(task_queue_factory));
  EXPECT_THAT(env.clock(), Ref(clock));
  EXPECT_THAT(env.event_log(), Ref(event_log));
}

TEST(EnvironmentTest, UsesLastProvidedUtility) {
  std::unique_ptr<FieldTrialsView> owned_field_trials1 =
      std::make_unique<FakeFieldTrials>();
  std::unique_ptr<FieldTrialsView> owned_field_trials2 =
      std::make_unique<FakeFieldTrials>();
  FieldTrialsView& field_trials2 = *owned_field_trials2;

  Environment env = EnvironmentFactory()
                        .With(std::move(owned_field_trials1))
                        .With(std::move(owned_field_trials2))
                        .Create();

  EXPECT_THAT(env.field_trials(), Ref(field_trials2));
}

TEST(EnvironmentTest, IgnoresProvidedNullptrUtility) {
  std::unique_ptr<FieldTrialsView> owned_field_trials1 =
      std::make_unique<FakeFieldTrials>();
  std::unique_ptr<FieldTrialsView> null_field_trials2 = nullptr;
  FieldTrialsView& field_trials1 = *owned_field_trials1;

  Environment env = EnvironmentFactory()
                        .With(std::move(owned_field_trials1))
                        .With(std::move(null_field_trials2))
                        .Create();

  EXPECT_THAT(env.field_trials(), Ref(field_trials1));
}

TEST(EnvironmentTest, KeepsUtilityAliveWhileEnvironmentIsAlive) {
  bool field_trials_destoyed = false;
  std::unique_ptr<FieldTrialsView> field_trials =
      std::make_unique<FakeFieldTrials>(
          /*on_destroyed=*/[&] { field_trials_destoyed = true; });

  absl::optional<Environment> env =
      EnvironmentFactory().With(std::move(field_trials)).Create();

  EXPECT_FALSE(field_trials_destoyed);
  env = absl::nullopt;
  EXPECT_TRUE(field_trials_destoyed);
}

TEST(EnvironmentTest, KeepsUtilityAliveWhileCopyOfEnvironmentIsAlive) {
  bool field_trials_destoyed = false;
  std::unique_ptr<FieldTrialsView> field_trials =
      std::make_unique<FakeFieldTrials>(
          /*on_destroyed=*/[&] { field_trials_destoyed = true; });

  absl::optional<Environment> env1 =
      EnvironmentFactory().With(std::move(field_trials)).Create();
  absl::optional<Environment> env2 = env1;

  EXPECT_FALSE(field_trials_destoyed);
  env1 = absl::nullopt;
  EXPECT_FALSE(field_trials_destoyed);
  env2 = absl::nullopt;
  EXPECT_TRUE(field_trials_destoyed);
}

TEST(EnvironmentTest, FactoryCanBeReusedToCreateDifferentEnvironments) {
  std::unique_ptr<TaskQueueFactory> owned_task_queue_factory =
      std::make_unique<FakeTaskQueueFactory>();
  std::unique_ptr<FieldTrialsView> owned_field_trials1 =
      std::make_unique<FakeFieldTrials>();
  std::unique_ptr<FieldTrialsView> owned_field_trials2 =
      std::make_unique<FakeFieldTrials>();
  TaskQueueFactory& task_queue_factory = *owned_task_queue_factory;
  FieldTrialsView& field_trials1 = *owned_field_trials1;
  FieldTrialsView& field_trials2 = *owned_field_trials2;

  EnvironmentFactory factory;
  factory.With(std::move(owned_task_queue_factory));
  Environment env1 = factory.With(std::move(owned_field_trials1)).Create();
  Environment env2 = factory.With(std::move(owned_field_trials2)).Create();

  // Environments share the same custom task queue factory.
  EXPECT_THAT(env1.task_queue_factory(), Ref(task_queue_factory));
  EXPECT_THAT(env2.task_queue_factory(), Ref(task_queue_factory));

  // Environments have different field trials.
  EXPECT_THAT(env1.field_trials(), Ref(field_trials1));
  EXPECT_THAT(env2.field_trials(), Ref(field_trials2));
}

TEST(EnvironmentTest, FactoryCanCreateNewEnvironmentFromExistingOne) {
  Environment env1 = EnvironmentFactory()
                         .With(std::make_unique<FakeTaskQueueFactory>())
                         .Create();
  Environment env2 = EnvironmentFactory(env1)
                         .With(std::make_unique<FakeFieldTrials>())
                         .Create();

  // Environments share the same default clock.
  EXPECT_THAT(env2.clock(), Ref(env1.clock()));

  // Environments share the same custom task queue factory.
  EXPECT_THAT(env2.task_queue_factory(), Ref(env1.task_queue_factory()));

  // Environments have different field trials.
  EXPECT_THAT(env2.field_trials(), Not(Ref(env1.field_trials())));
}

TEST(EnvironmentTest, DestroysUtilitiesInReverseProvidedOrder) {
  std::vector<std::string> destroyed;
  std::unique_ptr<FieldTrialsView> field_trials =
      std::make_unique<FakeFieldTrials>(
          /*on_destroyed=*/[&] { destroyed.push_back("field_trials"); });
  std::unique_ptr<TaskQueueFactory> task_queue_factory =
      std::make_unique<FakeTaskQueueFactory>(
          /*on_destroyed=*/[&] { destroyed.push_back("task_queue_factory"); });

  absl::optional<Environment> env = EnvironmentFactory()
                                        .With(std::move(field_trials))
                                        .With(std::move(task_queue_factory))
                                        .Create();

  ASSERT_THAT(destroyed, IsEmpty());
  env = absl::nullopt;
  EXPECT_THAT(destroyed, ElementsAre("task_queue_factory", "field_trials"));
}

}  // namespace
}  // namespace webrtc
