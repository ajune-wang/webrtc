/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <set>
#include <string>

#include "p2p/base/mdns_message.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"

#define ReadMDnsMessage(X, Y) ReadMDnsMessageTestCase(X, Y, sizeof(Y))
#define WriteMDnsMessageAndCompare(X, Y) \
  WriteMDnsMessageAndCompareWithTestCast(X, Y, sizeof(Y))

namespace {

const unsigned char kSingleQuestionForIPv4AddressWithUnicastResponse[] = {
    0x12, 0x34,                                // ID
    0x00, 0x00,                                // flags
    0x00, 0x01,                                // number of questions
    0x00, 0x00,                                // number of answer rr
    0x00, 0x00,                                // number of name server rr
    0x00, 0x00,                                // number of additional rr
    0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
    0x03, 0x6f, 0x72, 0x67,                    // org
    0x00,                                      // null label
    0x00, 0x01,                                // type A Record
    0x80, 0x01,                                // class IN, unicast response
};

const unsigned char
    kTwoQuestionsForIPv4AndIPv6AddressesWithMulticastResponse[] = {
        0x12, 0x34,  // ID
        0x00, 0x00,  // flags
        0x00, 0x02,  // number of questions
        0x00, 0x00,  // number of answer rr
        0x00, 0x00,  // number of name server rr
        0x00, 0x00,  // number of additional rr
        0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x34,  // 7, webrtc4
        0x03, 0x6f, 0x72, 0x67,                          // 3, org
        0x00,                                            // null label
        0x00, 0x01,                                      // type A Record
        0x00, 0x01,  // class IN, multicast response
        0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x36,  // webrtc6
        0x03, 0x6f, 0x72, 0x67,                          // org
        0x00,                                            // null label
        0x00, 0x1C,                                      // type AAAA Record
        0x00, 0x01,  // class IN, multicast response
};

const unsigned char kSingleAuthoritativeAnswerWithIPv4Address[] = {
    0x12, 0x34,                                // ID
    0x84, 0x00,                                // flags
    0x00, 0x00,                                // number of questions
    0x00, 0x01,                                // number of answer rr
    0x00, 0x00,                                // number of name server rr
    0x00, 0x00,                                // number of additional rr
    0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
    0x03, 0x6f, 0x72, 0x67,                    // org
    0x00,                                      // null label
    0x00, 0x01,                                // type A Record
    0x00, 0x01,                                // class IN
    0x00, 0x00, 0x00, 0x78,                    // TTL, 120 seconds
    0x00, 0x04,                                // rdlength, 32 bits
    0xC0, 0xA8, 0x00, 0x01,                    // 192.168.0.1
};

const unsigned char kTwoAuthoritativeAnswersWithIPv4AndIPv6Addresses[] = {
    0x12, 0x34,                                      // ID
    0x84, 0x00,                                      // flags
    0x00, 0x00,                                      // number of questions
    0x00, 0x02,                                      // number of answer rr
    0x00, 0x00,                                      // number of name server rr
    0x00, 0x00,                                      // number of additional rr
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x34,  // webrtc4
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x01,                                      // type A Record
    0x00, 0x01,                                      // class IN
    0x00, 0x00, 0x00, 0x3c,                          // TTL, 60 seconds
    0x00, 0x04,                                      // rdlength, 32 bits
    0xC0, 0xA8, 0x00, 0x01,                          // 192.168.0.1
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x36,  // webrtc6
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x1C,                                      // type AAAA Record
    0x00, 0x01,                                      // class IN
    0x00, 0x00, 0x00, 0x78,                          // TTL, 120 seconds
    0x00, 0x10,                                      // rdlength, 128 bits
    0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

bool ReadMDnsMessageTestCase(webrtc::MDnsMessage* msg,
                             const unsigned char* testcase,
                             size_t size) {
  rtc::ByteBufferReader buf(reinterpret_cast<const char*>(testcase), size);
  return msg->Read(&buf);
}

bool WriteMDnsMessageAndCompareWithTestCast(webrtc::MDnsMessage* msg,
                                            const unsigned char* testcase,
                                            size_t size) {
  rtc::ByteBufferWriter out;
  EXPECT_TRUE(msg->Write(&out));
  if (out.Length() != size) {
    RTC_LOG(INFO) << out.Length();
    RTC_LOG(INFO) << size;
    return false;
  }
  int len = static_cast<int>(out.Length());
  rtc::ByteBufferReader read_buf(out);
  std::string bytes;
  read_buf.ReadString(&bytes, len);
  return memcmp(bytes.c_str(), testcase, len) == 0;
}

bool GetQueriedNames(webrtc::MDnsMessage* msg, std::set<std::string>* names) {
  if (!msg->IsQuery() || !msg->GetNumQuestions()) {
    return false;
  }
  for (auto i = 0; i < msg->GetNumQuestions(); ++i) {
    names->insert(msg->GetQuestion(i)->GetName());
  }
  return true;
}

bool GetResolution(webrtc::MDnsMessage* msg,
                   std::map<std::string, rtc::IPAddress>* names) {
  if (msg->IsQuery() || !msg->GetNumAnswerRecords()) {
    return false;
  }
  for (auto i = 0; i < msg->GetNumAnswerRecords(); ++i) {
    auto* answer = msg->GetAnswerRecord(i);
    rtc::IPAddress resolved_addr;
    if (!answer->GetIPAddressFromRecordData(&resolved_addr)) {
      return false;
    }
    (*names)[answer->GetName()] = resolved_addr;
  }
  return true;
}

}  // namespace

namespace webrtc {

class MDnsMessageTest : public ::testing::Test {};

TEST_F(MDnsMessageTest, ReadMessageWithSingleQuestionForIPv4Address) {
  MDnsMessage msg;
  ASSERT_TRUE(
      ReadMDnsMessage(&msg, kSingleQuestionForIPv4AddressWithUnicastResponse));
  EXPECT_TRUE(msg.IsQuery());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(1, msg.GetNumQuestions());
  EXPECT_EQ(0, msg.GetNumAnswerRecords());
  EXPECT_EQ(0, msg.GetNumNameServerRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());
  EXPECT_TRUE(msg.ShouldUnicastResponse());

  auto* question = msg.GetQuestion(0);
  ASSERT_NE(nullptr, question);
  EXPECT_EQ(SectionDataType::kA, question->GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_TRUE(queried_names.size() == 1 &&
              queried_names.find("webrtc.org.") != queried_names.end());
}

TEST_F(MDnsMessageTest, ReadMessageWithTwoQuestionsForIPv4AndIPv6Addresses) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(
      &msg, kTwoQuestionsForIPv4AndIPv6AddressesWithMulticastResponse));
  EXPECT_TRUE(msg.IsQuery());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(2, msg.GetNumQuestions());
  EXPECT_EQ(0, msg.GetNumAnswerRecords());
  EXPECT_EQ(0, msg.GetNumNameServerRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());

  auto* question1 = msg.GetQuestion(0);
  auto* question2 = msg.GetQuestion(1);
  ASSERT_NE(nullptr, question1);
  ASSERT_NE(nullptr, question2);
  EXPECT_EQ(SectionDataType::kA, question1->GetType());
  EXPECT_EQ(SectionDataType::kAAAA, question2->GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_TRUE(queried_names.size() == 2 &&
              queried_names.find("webrtc4.org.") != queried_names.end() &&
              queried_names.find("webrtc6.org.") != queried_names.end());
}

TEST_F(MDnsMessageTest, ReadMessageWithSingleAnswerForIPv4Address) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(&msg, kSingleAuthoritativeAnswerWithIPv4Address));
  EXPECT_FALSE(msg.IsQuery());
  EXPECT_TRUE(msg.IsAuthoritative());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(0, msg.GetNumQuestions());
  EXPECT_EQ(1, msg.GetNumAnswerRecords());
  EXPECT_EQ(0, msg.GetNumNameServerRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());

  auto* answer = msg.GetAnswerRecord(0);
  ASSERT_NE(nullptr, answer);
  EXPECT_EQ(SectionDataType::kA, answer->GetType());
  EXPECT_EQ(120u, answer->ttl_seconds());

  std::map<std::string, rtc::IPAddress> resolution;
  EXPECT_TRUE(GetResolution(&msg, &resolution));
  EXPECT_TRUE(resolution.size() == 1 &&
              resolution.find("webrtc.org.") != resolution.end() &&
              resolution["webrtc.org."].ToString() == "192.168.0.1");
}

TEST_F(MDnsMessageTest, ReadMessageWithSingleAnswerForIPv6Address) {
  MDnsMessage msg;
  ASSERT_TRUE(
      ReadMDnsMessage(&msg, kTwoAuthoritativeAnswersWithIPv4AndIPv6Addresses));
  EXPECT_FALSE(msg.IsQuery());
  EXPECT_TRUE(msg.IsAuthoritative());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(0, msg.GetNumQuestions());
  EXPECT_EQ(2, msg.GetNumAnswerRecords());
  EXPECT_EQ(0, msg.GetNumNameServerRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());
  EXPECT_EQ(0, msg.GetNumAdditionalRecords());

  auto* answer1 = msg.GetAnswerRecord(0);
  auto* answer2 = msg.GetAnswerRecord(1);
  ASSERT_NE(nullptr, answer1);
  ASSERT_NE(nullptr, answer2);
  EXPECT_EQ(SectionDataType::kA, answer1->GetType());
  EXPECT_EQ(SectionDataType::kAAAA, answer2->GetType());
  EXPECT_EQ(60u, answer1->ttl_seconds());
  EXPECT_EQ(120u, answer2->ttl_seconds());

  std::map<std::string, rtc::IPAddress> resolution;
  EXPECT_TRUE(GetResolution(&msg, &resolution));
  EXPECT_TRUE(resolution.size() == 2 &&
              resolution.find("webrtc4.org.") != resolution.end() &&
              resolution["webrtc4.org."].ToString() == "192.168.0.1" &&
              resolution.find("webrtc6.org.") != resolution.end() &&
              resolution["webrtc6.org."].ToString() == "fd12:3456:789a:1::1");
}

TEST_F(MDnsMessageTest, WriteMessageWithSingleQuestionForIPv4Address) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(true);

  MDnsQuestion question;
  question.SetName("webrtc.org.");
  question.SetType(SectionDataType::kA);
  question.SetClass(SectionDataClass::kIN);
  question.SetUnicastResponse(true);
  msg.AddQuestion(question);

  EXPECT_TRUE(WriteMDnsMessageAndCompare(
      &msg, kSingleQuestionForIPv4AddressWithUnicastResponse));
}

TEST_F(MDnsMessageTest, WriteMessageWithTwoQuestionsForIPv4AndIPv6Addresses) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(true);

  MDnsQuestion question1;
  question1.SetName("webrtc4.org.");
  question1.SetType(SectionDataType::kA);
  question1.SetClass(SectionDataClass::kIN);
  msg.AddQuestion(question1);

  MDnsQuestion question2;
  question2.SetName("webrtc6.org.");
  question2.SetType(SectionDataType::kAAAA);
  question2.SetClass(SectionDataClass::kIN);
  msg.AddQuestion(question2);

  EXPECT_TRUE(WriteMDnsMessageAndCompare(
      &msg, kTwoQuestionsForIPv4AndIPv6AddressesWithMulticastResponse));
}

TEST_F(MDnsMessageTest, WriteMessageWithSingleAnswerToIPv4Address) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(false);
  msg.SetAuthoritative(true);

  MDnsResourceRecord answer;
  answer.SetName("webrtc.org.");
  answer.SetType(SectionDataType::kA);
  answer.SetClass(SectionDataClass::kIN);
  answer.SetIPAddressInRecordData(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  answer.set_ttl_seconds(120);
  msg.AddAnswerRecord(answer);

  EXPECT_TRUE(WriteMDnsMessageAndCompare(
      &msg, kSingleAuthoritativeAnswerWithIPv4Address));
}

TEST_F(MDnsMessageTest, WriteMessageWithTwoAnswersToIPv4AndIPv6Addresses) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(false);
  msg.SetAuthoritative(true);

  MDnsResourceRecord answer1;
  answer1.SetName("webrtc4.org.");
  answer1.SetType(SectionDataType::kA);
  answer1.SetClass(SectionDataClass::kIN);
  answer1.SetIPAddressInRecordData(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  answer1.set_ttl_seconds(60);
  msg.AddAnswerRecord(answer1);

  MDnsResourceRecord answer2;
  answer2.SetName("webrtc6.org.");
  answer2.SetType(SectionDataType::kAAAA);
  answer2.SetClass(SectionDataClass::kIN);
  answer2.SetIPAddressInRecordData(
      rtc::SocketAddress("fd12:3456:789a:1::1", 0).ipaddr());
  answer2.set_ttl_seconds(120);
  msg.AddAnswerRecord(answer2);

  EXPECT_TRUE(WriteMDnsMessageAndCompare(
      &msg, kTwoAuthoritativeAnswersWithIPv4AndIPv6Addresses));
}

}  // namespace webrtc
