/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/gunit.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace webrtc {
namespace {
const char kDummyExperiment[] = "WebRTC-DummyExperiment";

struct DummyExperiment {
  FieldTrialFlag enabled = FieldTrialFlag("Enabled");
  FieldTrialParameter<double> factor = FieldTrialParameter<double>("f", 0.5);
  FieldTrialParameter<int> retries = FieldTrialParameter<int>("r", 5);
  FieldTrialParameter<bool> ping = FieldTrialParameter<bool>("p", 0);
  FieldTrialParameter<std::string> hash =
      FieldTrialParameter<std::string>("h", "a80");

  explicit DummyExperiment(std::string field_trial) {
    ParseFieldTrial({&enabled, &factor, &retries, &ping, &hash}, field_trial);
  }
  DummyExperiment() {
    std::string trial_string = field_trial::FindFullName(kDummyExperiment);
    ParseFieldTrial({&enabled, &factor, &retries, &ping, &hash}, trial_string);
  }
};

enum class CustomEnum {
  kDefault = 0,
  kRed = 1,
  kBlue = 2,
};
}  // namespace

TEST(FieldTrialParserTest, ParsesValidParameters) {
  DummyExperiment exp("Enabled,f:-1.7,r:2,p:1,h:x7c");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}
TEST(FieldTrialParserTest, InitializesFromFieldTrial) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-OtherExperiment/Disabled/"
      "WebRTC-DummyExperiment/Enabled,f:-1.7,r:2,p:1,h:x7c/"
      "WebRTC-AnotherExperiment/Enabled,f:-3.1,otherstuff:beef/");
  DummyExperiment exp;
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}
TEST(FieldTrialParserTest, UsesDefaults) {
  DummyExperiment exp("");
  EXPECT_FALSE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "a80");
}
TEST(FieldTrialParserTest, CanHandleMixedInput) {
  DummyExperiment exp("p:true,h:,Enabled");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "");
}
TEST(FieldTrialParserTest, ParsesDoubleParameter) {
  FieldTrialParameter<double> double_param("f", 0.0);
  ParseFieldTrial({&double_param}, "f:45%");
  EXPECT_EQ(double_param.Get(), 0.45);
  ParseFieldTrial({&double_param}, "f:34 %");
  EXPECT_EQ(double_param.Get(), 0.34);
  ParseFieldTrial({&double_param}, "f:0.67");
  EXPECT_EQ(double_param.Get(), 0.67);
}
TEST(FieldTrialParserTest, IgnoresNewKey) {
  DummyExperiment exp("Disabled,r:-11,foo");
  EXPECT_FALSE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), -11);
}
TEST(FieldTrialParserTest, IgnoresInvalid) {
  DummyExperiment exp("Enabled,f,p:,r:%,,:foo,h");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "a80");
}
TEST(FieldTrialParserTest, IgnoresOutOfRange) {
  FieldTrialConstrained<double> low("low", 10, absl::nullopt, 100);
  FieldTrialConstrained<double> high("high", 10, 5, absl::nullopt);
  ParseFieldTrial({&low, &high}, "low:1000,high:0");
  EXPECT_EQ(low.Get(), 10);
  EXPECT_EQ(high.Get(), 10);
  ParseFieldTrial({&low, &high}, "low:inf,high:nan");
  EXPECT_EQ(low.Get(), 10);
  EXPECT_EQ(high.Get(), 10);
  ParseFieldTrial({&low, &high}, "low:20,high:20");
  EXPECT_EQ(low.Get(), 20);
  EXPECT_EQ(high.Get(), 20);
}
TEST(FieldTrialParserTest, ReadsValuesFromFieldWithoutKey) {
  FieldTrialFlag enabled("Enabled");
  FieldTrialParameter<int> req("", 10);
  ParseFieldTrial({&enabled, &req}, "Enabled,20");
  EXPECT_EQ(req.Get(), 20);
  ParseFieldTrial({&req}, "30");
  EXPECT_EQ(req.Get(), 30);
}
TEST(FieldTrialParserTest, ParsesOptionalParameters) {
  FieldTrialOptional<int> max_count("c", absl::nullopt);
  ParseFieldTrial({&max_count}, "");
  EXPECT_FALSE(max_count.GetOptional().has_value());
  ParseFieldTrial({&max_count}, "c:10");
  EXPECT_EQ(max_count.GetOptional().value(), 10);
  ParseFieldTrial({&max_count}, "c");
  EXPECT_FALSE(max_count.GetOptional().has_value());
  ParseFieldTrial({&max_count}, "c:20");
  EXPECT_EQ(max_count.GetOptional().value(), 20);
  ParseFieldTrial({&max_count}, "c:");
  EXPECT_EQ(max_count.GetOptional().value(), 20);
  FieldTrialOptional<std::string> optional_string("s", std::string("ab"));
  ParseFieldTrial({&optional_string}, "s:");
  EXPECT_EQ(optional_string.GetOptional().value(), "");
  ParseFieldTrial({&optional_string}, "s");
  EXPECT_FALSE(optional_string.GetOptional().has_value());
}
TEST(FieldTrialParserTest, ParsesCustomEnumParameter) {
  FieldTrialEnum<CustomEnum> my_enum("e", CustomEnum::kDefault,
                                     {{"default", CustomEnum::kDefault},
                                      {"red", CustomEnum::kRed},
                                      {"blue", CustomEnum::kBlue}});
  ParseFieldTrial({&my_enum}, "");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kDefault);
  ParseFieldTrial({&my_enum}, "e:red");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kRed);
  ParseFieldTrial({&my_enum}, "e:2");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kBlue);
  ParseFieldTrial({&my_enum}, "e:5");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kBlue);
}
TEST(FieldTrialParserTest, ParsesListParameter) {
  FieldTrialList<int> my_list("l", {5});
  EXPECT_THAT(my_list.Get(), ElementsAre(5));
  // If one element is invalid the list is unchanged.
  ParseFieldTrial({&my_list}, "l:1|2|hat");
  EXPECT_THAT(my_list.Get(), ElementsAre(5));
  ParseFieldTrial({&my_list}, "l");
  EXPECT_THAT(my_list.Get(), IsEmpty());
  ParseFieldTrial({&my_list}, "l:1|2|3");
  EXPECT_THAT(my_list.Get(), ElementsAre(1, 2, 3));
  ParseFieldTrial({&my_list}, "l:-1");
  EXPECT_THAT(my_list.Get(), ElementsAre(-1));

  FieldTrialList<std::string> another_list("l", {"hat"});
  EXPECT_THAT(another_list.Get(), ElementsAre("hat"));
  ParseFieldTrial({&another_list}, "l");
  EXPECT_THAT(another_list.Get(), IsEmpty());
  ParseFieldTrial({&another_list}, "l:");
  EXPECT_THAT(another_list.Get(), ElementsAre(""));
  ParseFieldTrial({&another_list}, "l:scarf|hat|mittens");
  EXPECT_THAT(another_list.Get(), ElementsAre("scarf", "hat", "mittens"));
  ParseFieldTrial({&another_list}, "l:scarf");
  EXPECT_THAT(another_list.Get(), ElementsAre("scarf"));
}

TEST(FieldTrialParserTest, ParsesListOfTuples) {
  struct Garment {
    Garment(int p, std::string c, std::string g)
        : price(p), color(c), garment(g) {}

    int price = 1;
    std::string color = "mauve";
    std::string garment = "cravatte";

    bool operator==(const Garment& other) const {
      return price == other.price && color == other.color &&
             garment == other.garment;
    }
  };

  FieldTrialList<int> price("price");
  FieldTrialList<std::string> color("color");
  FieldTrialList<std::string> garment("garment");
  FieldTrialList<std::string> wrong_length("other");

  ParseFieldTrial({&price, &color, &garment, &wrong_length},
                  "color:mauve|red|gold,"
                  "garment:hat|hat|crown,"
                  "price:10|20|30,"
                  "other:asdf");

  std::vector<Garment> out = {{1, "blue", "boot"}, {2, "red", "glove"}};

  // Normal usage.
  ASSERT_TRUE(
      combineLists({TLW(color, [](Garment* g) { return &g->color; }),
                    TLW(garment, [](Garment* g) { return &g->garment; }),
                    TLW(price, [](Garment* g) { return &g->price; })},
                   Garment{-1, "gray", "cravatte"}, &out));

  ASSERT_THAT(
      out, ElementsAre(Garment(10, "mauve", "hat"), Garment(20, "red", "hat"),
                       Garment(30, "gold", "crown")));

  // One FieldTrialList has the wrong length, so out remains unchanged.
  out = {{1, "blue", "boot"}, {2, "red", "glove"}};
  ASSERT_FALSE(
      combineLists({TLW(color, [](Garment* g) { return &g->color; }),
                    TLW(garment, [](Garment* g) { return &g->garment; }),
                    TLW(wrong_length, [](Garment* g) { return &g->garment; })},
                   Garment{-1, "gray", "cravatte"}, &out));

  ASSERT_THAT(
      out, ElementsAre(Garment(1, "blue", "boot"), Garment(2, "red", "glove")));

  // One List is missing, so we use the default value from sample.
  ASSERT_TRUE(
      combineLists({TLW(price, [](Garment* g) { return &g->price; }),
                    TLW(garment, [](Garment* g) { return &g->garment; })},
                   Garment{-1, "gray", "cravatte"}, &out));

  ASSERT_THAT(
      out, ElementsAre(Garment(10, "gray", "hat"), Garment(20, "gray", "hat"),
                       Garment(30, "gray", "crown")));

  // User haven't provided values for all our lists, use defaults for the
  // missing ones.
  out = {};
  FieldTrialList<int> price2("price");
  FieldTrialList<std::string> color2("color");
  FieldTrialList<std::string> garment2("garment");
  ParseFieldTrial({&price2, &color2, &garment2},
                  "color:mauve|red|gold,"
                  "garment:hat|hat|crown");
  ASSERT_TRUE(
      combineLists({TLW(price2, [](Garment* g) { return &g->price; }),
                    TLW(color2, [](Garment* g) { return &g->color; }),
                    TLW(garment2, [](Garment* g) { return &g->garment; })},
                   Garment{-1, "gray", "cravatte"}, &out));

  ASSERT_THAT(
      out, ElementsAre(Garment(-1, "mauve", "hat"), Garment(-1, "red", "hat"),
                       Garment(-1, "gold", "crown")));

  // User haven't provided values for any lists, so out is unchanged.
  out = {{1, "blue", "boot"}, {2, "red", "glove"}};
  FieldTrialList<int> price3("price");
  FieldTrialList<std::string> color3("color");
  FieldTrialList<std::string> garment3("garment");
  ParseFieldTrial({&price3, &color3, &garment3}, "");
  ASSERT_TRUE(
      combineLists({TLW(price3, [](Garment* g) { return &g->price; }),
                    TLW(color3, [](Garment* g) { return &g->color; }),
                    TLW(garment3, [](Garment* g) { return &g->garment; })},
                   Garment{-1, "gray", "cravatte"}, &out));

  ASSERT_THAT(
      out, ElementsAre(Garment(1, "blue", "boot"), Garment(2, "red", "glove")));
}

}  // namespace webrtc
