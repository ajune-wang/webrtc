/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MDNS_MESSAGE_H_
#define P2P_BASE_MDNS_MESSAGE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "rtc_base/bytebuffer.h"
#include "rtc_base/ipaddress.h"

namespace webrtc {

// We use "section data" to denote either a question or a resource record.
//
// RFC 1035 Section 3.2.2.
enum class SectionDataType {
  kA,
  kAAAA,
  // Only the above types are processed in the current implementation.
  kUnsupported,
};

// RFC 1035 Section 3.2.4.
enum class SectionDataClass {
  kIN,
  kUnsupported,
};

struct MDnsHeader {
  MDnsHeader();
  bool Read(rtc::ByteBufferReader* buf);
  void Write(rtc::ByteBufferWriter* buf) const;

  void SetQueryOrResponse(bool is_query);
  void SetAuthoritative(bool is_authoritative);
  bool IsAuthoritative() const;

  bool IsQuery() const;

  uint16_t id = 0;
  uint16_t flags = 0;
  uint16_t qdcount = 0;
  uint16_t ancount = 0;
  uint16_t nscount = 0;
  uint16_t arcount = 0;
};

class MDnsSectionData {
 public:
  MDnsSectionData();
  MDnsSectionData(const MDnsSectionData& other);
  virtual ~MDnsSectionData();
  virtual bool Read(rtc::ByteBufferReader* buf) = 0;
  virtual bool Write(rtc::ByteBufferWriter* buf) const = 0;

  // Set the name by a sequence of label, e.g. {"webrtc", "org"}.
  void SetName(const std::vector<std::string>& labels) { name_ = labels; }
  // Set the name by a fully qualified domain name, e.g. "webrtc.org.".
  void SetName(const std::string& name);
  // Returns the fully qualified domain name in the section data, QNAME in a
  // question or NAME in a resource record.
  std::string GetName() const;

  void SetType(SectionDataType type);
  SectionDataType GetType() const;
  void SetClass(SectionDataClass cls);
  SectionDataClass GetClass() const;

 protected:
  std::vector<std::string> name_;  // As a sequence of labels.
  uint16_t type_;
  uint16_t class_;
};

class MDnsQuestion : public MDnsSectionData {
 public:
  MDnsQuestion();
  MDnsQuestion(const MDnsQuestion& other);
  ~MDnsQuestion() override;

  bool Read(rtc::ByteBufferReader* buf) override;
  bool Write(rtc::ByteBufferWriter* buf) const override;

  void SetUnicastResponse(bool should_unicast);
  bool ShouldUnicastResponse() const;
};

class MDnsResourceRecord : public MDnsSectionData {
 public:
  MDnsResourceRecord();
  MDnsResourceRecord(const MDnsResourceRecord& other);
  ~MDnsResourceRecord() override;

  bool Read(rtc::ByteBufferReader* buf) override;
  bool Write(rtc::ByteBufferWriter* buf) const override;

  void set_ttl_seconds(uint32_t ttl_seconds) { ttl_seconds_ = ttl_seconds; }
  uint32_t ttl_seconds() const { return ttl_seconds_; }
  void SetIPAddressInRecordData(const rtc::IPAddress& address);
  // Returns true if the record is of type A or AAAA and the record has a valid
  // IPv4 or IPv6 address; false otherwise. Stores the valid IP in |address|.
  bool GetIPAddressFromRecordData(rtc::IPAddress* address) const;

 private:
  // The list of methods reading and writing rdata can grow as we support more
  // types of rdata.
  bool ReadARData(rtc::ByteBufferReader* buf);
  void WriteARData(rtc::ByteBufferWriter* buf) const;

  bool ReadQuadARData(rtc::ByteBufferReader* buf);
  void WriteQuadARData(rtc::ByteBufferWriter* buf) const;

  uint32_t ttl_seconds_;
  uint16_t rdlength_;
  std::string rdata_;
};

class MDnsMessage {
 public:
  // RFC 1035, Section 4.1.
  enum class Section { kQuestion, kAnswer, kAuthority, kAdditional };

  MDnsMessage();
  ~MDnsMessage();
  // Reads the mDNS message in |buf| and populates the corresponding fields in
  // MDnsMessage.
  bool Read(rtc::ByteBufferReader* buf);
  // Write an mDNS message to |buf| based on the fields in MDnsMessage.
  bool Write(rtc::ByteBufferWriter* buf) const;

  void SetId(uint16_t id) { header_.id = id; }
  uint16_t GetId() const { return header_.id; }

  void SetQueryOrResponse(bool is_query) {
    header_.SetQueryOrResponse(is_query);
  }
  bool IsQuery() const { return header_.IsQuery(); }

  void SetAuthoritative(bool is_authoritative) {
    header_.SetAuthoritative(is_authoritative);
  }
  bool IsAuthoritative() const { return header_.IsAuthoritative(); }

  // Returns true if the message is a query and the unicast response is
  // preferred. False otherwise.
  bool ShouldUnicastResponse() const;

  void AddQuestion(const MDnsQuestion& question);
  // TODO(qingsi): Implement AddXRecord for name server and additional records.
  void AddAnswerRecord(const MDnsResourceRecord& answer);

  int GetNumQuestions() const { return header_.qdcount; }
  int GetNumAnswerRecords() const { return header_.ancount; }
  int GetNumNameServerRecords() const { return header_.nscount; }
  int GetNumAdditionalRecords() const { return header_.arcount; }

  MDnsQuestion* GetQuestion(int index);
  MDnsResourceRecord* GetAnswerRecord(int index);
  MDnsResourceRecord* GetNameServerRecord(int index);
  MDnsResourceRecord* GetAdditionalRecord(int index);

 private:
  int GetNumSectionData(Section section) const;
  MDnsResourceRecord* GetResourceRecord(Section section, int index);

  MDnsHeader header_;
  std::vector<MDnsQuestion> question_section_;
  std::vector<MDnsResourceRecord> answer_section_;
  std::vector<MDnsResourceRecord> authority_section_;
  std::vector<MDnsResourceRecord> additional_section_;
};

}  // namespace webrtc

#endif  // P2P_BASE_MDNS_MESSAGE_H_
