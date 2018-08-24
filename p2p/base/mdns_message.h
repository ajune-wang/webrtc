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

// This file contains classes to read and write mDNSs message defined in RFC
// 6762 and RFC 1025 (DNS messages). Note that it is recommended by RFC 6762 to
// use the name compression scheme defined in RFC 1035 whenever possible. We
// currently only implement the capability of reading compressed names in mDNS
// messages in MDnsMessage::Read(); however, the MDnsMessage::Write() does not
// support name compression yet.
//
// Fuzzer tests MUST always be performed after changes made to this file.
//
// TODO(qingsi): Implement name compression when writing mDNS messages.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "rtc_base/bytebuffer.h"
#include "rtc_base/ipaddress.h"

namespace webrtc {

// We use "section entry" to denote either a question or a resource record.
//
// RFC 1035 Section 3.2.2.
enum class SectionEntryType {
  kA,
  kAAAA,
  // Only the above types are processed in the current implementation.
  kUnsupported,
};

// RFC 1035 Section 3.2.4.
enum class SectionEntryClass {
  kIN,
  kUnsupported,
};

class MDnsMessageBufferReader;

// RFC 1035, Section 4.1.1.
struct MDnsHeader {
  MDnsHeader();
  bool Read(MDnsMessageBufferReader* buf);
  void Write(rtc::ByteBufferWriter* buf) const;

  void SetQueryOrResponse(bool is_query);
  bool IsQuery() const;
  void SetAuthoritative(bool is_authoritative);
  bool IsAuthoritative() const;

  uint16_t id = 0;
  uint16_t flags = 0;
  // Number of entries in the question section.
  uint16_t qdcount = 0;
  // Number of resource records in the answer section.
  uint16_t ancount = 0;
  // Number of name server resource records in the authority records section.
  uint16_t nscount = 0;
  // Number of resource records in the additional records section.
  uint16_t arcount = 0;
};

// Entries in each section after the header share a common structure. Note that
// this is not a concept defined in RFC 1035.
class MDnsSectionEntry {
 public:
  MDnsSectionEntry();
  MDnsSectionEntry(const MDnsSectionEntry& other);
  virtual ~MDnsSectionEntry();
  virtual bool Read(MDnsMessageBufferReader* buf) = 0;
  virtual bool Write(rtc::ByteBufferWriter* buf) const = 0;

  void SetName(const std::string& name) { name_ = name; }
  // Returns the fully qualified domain name in the section entry, i.e., QNAME
  // in a question or NAME in a resource record.
  std::string GetName() const { return name_; }

  void SetType(SectionEntryType type);
  SectionEntryType GetType() const;
  void SetClass(SectionEntryClass cls);
  SectionEntryClass GetClass() const;

 protected:
  std::string name_;  // Fully qualified domain name.
  uint16_t type_;
  uint16_t class_;
};

// RFC 1035, Section 4.1.2.
class MDnsQuestion : public MDnsSectionEntry {
 public:
  MDnsQuestion();
  MDnsQuestion(const MDnsQuestion& other);
  ~MDnsQuestion() override;

  bool Read(MDnsMessageBufferReader* buf) override;
  bool Write(rtc::ByteBufferWriter* buf) const override;

  void SetUnicastResponse(bool should_unicast);
  bool ShouldUnicastResponse() const;
};

// RFC 1035, Section 4.1.3.
class MDnsResourceRecord : public MDnsSectionEntry {
 public:
  MDnsResourceRecord();
  MDnsResourceRecord(const MDnsResourceRecord& other);
  ~MDnsResourceRecord() override;

  bool Read(MDnsMessageBufferReader* buf) override;
  bool Write(rtc::ByteBufferWriter* buf) const override;

  void SetTtlSeconds(uint32_t ttl_seconds) { ttl_seconds_ = ttl_seconds; }
  uint32_t GetTtlSeconds() const { return ttl_seconds_; }
  // Returns true if |address| is in the address family AF_INET or AF_INET6 and
  // |address| has a valid IPv4 or IPv6 address; false otherwise.
  bool SetIPAddressInRecordData(const rtc::IPAddress& address);
  // Returns true if the record is of type A or AAAA and the record has a valid
  // IPv4 or IPv6 address; false otherwise. Stores the valid IP in |address|.
  bool GetIPAddressFromRecordData(rtc::IPAddress* address) const;

 private:
  // The list of methods reading and writing rdata can grow as we support more
  // types of rdata.
  bool ReadARData(MDnsMessageBufferReader* buf);
  void WriteARData(rtc::ByteBufferWriter* buf) const;

  bool ReadQuadARData(MDnsMessageBufferReader* buf);
  void WriteQuadARData(rtc::ByteBufferWriter* buf) const;

  uint32_t ttl_seconds_;
  uint16_t rdlength_;
  std::string rdata_;
};

class MDnsMessage {
 public:
  struct MessageInfo {
    MessageInfo();
    MessageInfo(const MessageInfo& other);
    ~MessageInfo();

    const char* message_start;
    size_t message_size;
    // Positions of pointer fields given by their offsets from the start of the
    // message.
    std::set<size_t> pos_of_pointers;
  };
  // RFC 1035, Section 4.1.
  enum class Section { kQuestion, kAnswer, kAuthority, kAdditional };

  MDnsMessage();
  ~MDnsMessage();
  // Reads the mDNS message in |buf| and populates the corresponding fields in
  // MDnsMessage.
  bool Read(MDnsMessageBufferReader* buf);
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

  const std::vector<MDnsQuestion>& question_section() const {
    return question_section_;
  }
  const std::vector<MDnsResourceRecord>& answer_section() const {
    return answer_section_;
  }
  const std::vector<MDnsResourceRecord>& authority_section() const {
    return authority_section_;
  }
  const std::vector<MDnsResourceRecord>& additional_section() const {
    return additional_section_;
  }

 private:
  MDnsHeader header_;
  std::vector<MDnsQuestion> question_section_;
  std::vector<MDnsResourceRecord> answer_section_;
  std::vector<MDnsResourceRecord> authority_section_;
  std::vector<MDnsResourceRecord> additional_section_;
};

// A simple subclass of the ByteBufferReader that holds the starting address of
// the message and its length, so that we can recall prior names when reading
// compressed names defined by pointers using the offset from the beginning of
// the message. See RFC 1035, Section 4.1.4.
class MDnsMessageBufferReader : public rtc::ByteBufferReader {
 public:
  explicit MDnsMessageBufferReader(
      const MDnsMessage::MessageInfo& message_info);
  MDnsMessageBufferReader(const char* read_start_from,
                          size_t len_to_read,
                          const MDnsMessage::MessageInfo& message_info);
  ~MDnsMessageBufferReader();

  // Read the fully qualified domain name from a section entry, i.e., QNAME in a
  // question or NAME in a resource record.
  bool ReadDomainName(std::string* name);

 private:
  const char* message_start() const { return message_info_.message_start; }
  size_t message_size() const { return message_info_.message_size; }

  bool PointerIsVisited(size_t pos) const;
  void AddPointerVisited(size_t pos);

  MDnsMessage::MessageInfo message_info_;
};

}  // namespace webrtc

#endif  // P2P_BASE_MDNS_MESSAGE_H_
