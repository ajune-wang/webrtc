/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "rtc_base/gunit.h"
#include "rtc_base/httpbase.h"
#include "rtc_base/testutils.h"

namespace rtc {

const char* const kHttpResponse =
    "HTTP/1.1 200\r\n"
    "Connection: Keep-Alive\r\n"
    "Content-Type: text/plain\r\n"
    "Proxy-Authorization: 42\r\n"
    "Transfer-Encoding: chunked\r\n"
    "\r\n"
    "00000008\r\n"
    "Goodbye!\r\n"
    "0\r\n\r\n";

const char* const kHttpEmptyResponse =
    "HTTP/1.1 200\r\n"
    "Connection: Keep-Alive\r\n"
    "Content-Length: 0\r\n"
    "Proxy-Authorization: 42\r\n"
    "\r\n";

class HttpBaseTest : public testing::Test, public IHttpNotify {
 public:
  enum EventType { E_HEADER_COMPLETE, E_COMPLETE, E_CLOSED };
  struct Event {
    EventType event;
    bool chunked;
    size_t data_size;
    HttpMode mode;
    HttpError err;
  };
  HttpBaseTest() : http_stream(nullptr) {}

  void TearDown() override {
    delete http_stream;
    // Avoid an ASSERT, in case a test doesn't clean up properly
    base.abort(HE_NONE);
  }

  HttpError onHttpHeaderComplete(bool chunked, size_t& data_size) override {
    RTC_LOG_F(LS_VERBOSE) << "chunked: " << chunked << " size: " << data_size;
    Event e = {E_HEADER_COMPLETE, chunked, data_size, HM_NONE, HE_NONE};
    events.push_back(e);
    return HE_NONE;
  }
  void onHttpComplete(HttpMode mode, HttpError err) override {
    RTC_LOG_F(LS_VERBOSE) << "mode: " << mode << " err: " << err;
    Event e = {E_COMPLETE, false, 0, mode, err};
    events.push_back(e);
  }
  void onHttpClosed(HttpError err) override {
    RTC_LOG_F(LS_VERBOSE) << "err: " << err;
    Event e = {E_CLOSED, false, 0, HM_NONE, err};
    events.push_back(e);
  }

  void SetupSource(const char* response);

  void VerifyHeaderComplete(size_t event_count, bool empty_doc);

  void SetupDocument();
  void VerifySourceContents(const char* expected_data,
                            size_t expected_length = SIZE_UNKNOWN);

  void VerifyTransferComplete(HttpMode mode, HttpError error);

  HttpBase base;
  HttpResponseData data;

  // The source of http data, and source events
  webrtc::testing::StreamSource src;
  std::vector<Event> events;

  // Stream events
  StreamInterface* http_stream;
  webrtc::testing::StreamSink sink;
};

void HttpBaseTest::SetupSource(const char* http_data) {
  RTC_LOG_F(LS_VERBOSE) << "Enter";

  src.SetState(SS_OPENING);
  src.QueueString(http_data);

  base.notify(this);
  base.attach(&src);
  EXPECT_TRUE(events.empty());

  src.SetState(SS_OPEN);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(E_COMPLETE, events[0].event);
  EXPECT_EQ(HM_CONNECT, events[0].mode);
  EXPECT_EQ(HE_NONE, events[0].err);
  events.clear();

  RTC_LOG_F(LS_VERBOSE) << "Exit";
}

void HttpBaseTest::VerifyHeaderComplete(size_t event_count, bool empty_doc) {
  RTC_LOG_F(LS_VERBOSE) << "Enter";

  ASSERT_EQ(event_count, events.size());
  EXPECT_EQ(E_HEADER_COMPLETE, events[0].event);

  std::string header;
  EXPECT_EQ(HVER_1_1, data.version);
  EXPECT_EQ(static_cast<uint32_t>(HC_OK), data.scode);
  EXPECT_TRUE(data.hasHeader(HH_PROXY_AUTHORIZATION, &header));
  EXPECT_EQ("42", header);
  EXPECT_TRUE(data.hasHeader(HH_CONNECTION, &header));
  EXPECT_EQ("Keep-Alive", header);

  if (empty_doc) {
    EXPECT_FALSE(events[0].chunked);
    EXPECT_EQ(0U, events[0].data_size);

    EXPECT_TRUE(data.hasHeader(HH_CONTENT_LENGTH, &header));
    EXPECT_EQ("0", header);
  } else {
    EXPECT_TRUE(events[0].chunked);
    EXPECT_EQ(SIZE_UNKNOWN, events[0].data_size);

    EXPECT_TRUE(data.hasHeader(HH_CONTENT_TYPE, &header));
    EXPECT_EQ("text/plain", header);
    EXPECT_TRUE(data.hasHeader(HH_TRANSFER_ENCODING, &header));
    EXPECT_EQ("chunked", header);
  }
  RTC_LOG_F(LS_VERBOSE) << "Exit";
}

void HttpBaseTest::SetupDocument() {
  RTC_LOG_F(LS_VERBOSE) << "Enter";
  src.SetState(SS_OPEN);

  base.notify(this);
  base.attach(&src);
  EXPECT_TRUE(events.empty());

  data.setHeader(HH_CONTENT_LENGTH, "0");

  data.scode = HC_OK;
  data.setHeader(HH_PROXY_AUTHORIZATION, "42");
  data.setHeader(HH_CONNECTION, "Keep-Alive");
  RTC_LOG_F(LS_VERBOSE) << "Exit";
}

void HttpBaseTest::VerifySourceContents(const char* expected_data,
                                        size_t expected_length) {
  RTC_LOG_F(LS_VERBOSE) << "Enter";
  if (SIZE_UNKNOWN == expected_length) {
    expected_length = strlen(expected_data);
  }
  std::string contents = src.ReadData();
  EXPECT_EQ(expected_length, contents.length());
  EXPECT_TRUE(0 == memcmp(expected_data, contents.data(), expected_length));
  RTC_LOG_F(LS_VERBOSE) << "Exit";
}

void HttpBaseTest::VerifyTransferComplete(HttpMode mode, HttpError error) {
  RTC_LOG_F(LS_VERBOSE) << "Enter";
  // Verify that http operation has completed
  ASSERT_TRUE(events.size() > 0);
  size_t last_event = events.size() - 1;
  EXPECT_EQ(E_COMPLETE, events[last_event].event);
  EXPECT_EQ(mode, events[last_event].mode);
  EXPECT_EQ(error, events[last_event].err);
  RTC_LOG_F(LS_VERBOSE) << "Exit";
}

//
// Tests
//

TEST_F(HttpBaseTest, SupportsSendNoDocument) {
  // Queue response document
  SetupDocument();

  // Begin send
  base.send(&data);

  // Send completed successfully
  VerifyTransferComplete(HM_SEND, HE_NONE);
  VerifySourceContents(kHttpEmptyResponse);
}

TEST_F(HttpBaseTest, SupportsReceiveViaDocumentPush) {
  // Queue response document
  SetupSource(kHttpResponse);

  // Begin receive
  base.recv(&data);

  // Document completed successfully
  VerifyHeaderComplete(2, false);
  VerifyTransferComplete(HM_RECV, HE_NONE);
}

}  // namespace rtc
