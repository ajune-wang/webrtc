/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/mdns_message.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringencode.h"

namespace {
const uint16_t kMDnsFlagMaskQueryOrResponse = 0x8000;
const uint16_t kMDnsFlagMaskAuthoritative = 0x0400;
const uint16_t kMDnsQClassMaskUnicastResponse = 0x8000;

bool ReadDomainName(rtc::ByteBufferReader* buf,
                    std::vector<std::string>* name) {
  uint8_t label_length;
  name->clear();
  if (!buf->ReadUInt8(&label_length)) {
    return false;
  }
  while (label_length) {
    std::string label;
    if (!buf->ReadString(&label, label_length)) {
      return false;
    }
    name->push_back(label);
    if (!buf->ReadUInt8(&label_length)) {
      return false;
    }
  }
  return true;
}

void WriteDomainName(rtc::ByteBufferWriter* buf,
                     const std::vector<std::string> name) {
  for (const auto& label : name) {
    buf->WriteUInt8(label.length());
    buf->WriteString(label);
  }
  buf->WriteUInt8(0);
}

}  // namespace

namespace webrtc {

MDnsHeader::MDnsHeader() = default;

void MDnsHeader::SetQueryOrResponse(bool is_query) {
  if (is_query) {
    flags &= ~kMDnsFlagMaskQueryOrResponse;
  } else {
    flags |= kMDnsFlagMaskQueryOrResponse;
  }
}

void MDnsHeader::SetAuthoritative(bool is_authoritative) {
  if (is_authoritative) {
    flags |= kMDnsFlagMaskAuthoritative;
  } else {
    flags &= ~kMDnsFlagMaskAuthoritative;
  }
}

bool MDnsHeader::IsAuthoritative() const {
  return flags & kMDnsFlagMaskAuthoritative;
}

bool MDnsHeader::Read(rtc::ByteBufferReader* buf) {
  if (!buf->ReadUInt16(&id) || !buf->ReadUInt16(&flags) ||
      !buf->ReadUInt16(&qdcount) || !buf->ReadUInt16(&ancount) ||
      !buf->ReadUInt16(&nscount) || !buf->ReadUInt16(&arcount)) {
    RTC_LOG(LS_ERROR) << "Invalid mDNS header.";
    return false;
  }
  return true;
}

void MDnsHeader::Write(rtc::ByteBufferWriter* buf) const {
  buf->WriteUInt16(id);
  buf->WriteUInt16(flags);
  buf->WriteUInt16(qdcount);
  buf->WriteUInt16(ancount);
  buf->WriteUInt16(nscount);
  buf->WriteUInt16(arcount);
}

bool MDnsHeader::IsQuery() const {
  return !(flags & kMDnsFlagMaskQueryOrResponse);
}

MDnsSectionData::MDnsSectionData() = default;
MDnsSectionData::~MDnsSectionData() = default;
MDnsSectionData::MDnsSectionData(const MDnsSectionData& other) = default;

void MDnsSectionData::SetName(const std::string& name) {
  // The null label of the root level is discard in rtc::tokenize.
  rtc::tokenize(name, '.', &name_);
}

std::string MDnsSectionData::GetName() const {
  // Converts a sequence of labels to a fully qualified domain name.
  std::string fqdn;
  for (const auto& label : name_) {
    fqdn += label + ".";
  }
  return fqdn;
}

void MDnsSectionData::SetType(SectionDataType type) {
  switch (type) {
    case SectionDataType::kA:
      type_ = 1;
      return;
    case SectionDataType::kAAAA:
      type_ = 28;
      return;
    default:
      RTC_NOTREACHED();
  }
}

SectionDataType MDnsSectionData::GetType() const {
  switch (type_) {
    case 1:
      return SectionDataType::kA;
    case 28:
      return SectionDataType::kAAAA;
    default:
      return SectionDataType::kUnsupported;
  }
}

void MDnsSectionData::SetClass(SectionDataClass cls) {
  switch (cls) {
    case SectionDataClass::kIN:
      class_ = 1;
      return;
    default:
      RTC_NOTREACHED();
  }
}

SectionDataClass MDnsSectionData::GetClass() const {
  switch (class_) {
    case 1:
      return SectionDataClass::kIN;
    default:
      return SectionDataClass::kUnsupported;
  }
}

MDnsQuestion::MDnsQuestion() = default;
MDnsQuestion::MDnsQuestion(const MDnsQuestion& other) = default;
MDnsQuestion::~MDnsQuestion() = default;

bool MDnsQuestion::Read(rtc::ByteBufferReader* buf) {
  if (!ReadDomainName(buf, &name_)) {
    RTC_LOG(LS_ERROR) << "Invalid name.";
    return false;
  }
  if (!buf->ReadUInt16(&type_) || !buf->ReadUInt16(&class_)) {
    RTC_LOG(LS_ERROR) << "Invalid type and class.";
    return false;
  }
  return true;
}

bool MDnsQuestion::Write(rtc::ByteBufferWriter* buf) const {
  WriteDomainName(buf, name_);
  buf->WriteUInt16(type_);
  buf->WriteUInt16(class_);
  return true;
}

void MDnsQuestion::SetUnicastResponse(bool should_unicast) {
  if (should_unicast) {
    class_ |= kMDnsQClassMaskUnicastResponse;
  } else {
    class_ &= ~kMDnsQClassMaskUnicastResponse;
  }
}

bool MDnsQuestion::ShouldUnicastResponse() const {
  return class_ & kMDnsQClassMaskUnicastResponse;
}

MDnsResourceRecord::MDnsResourceRecord() = default;
MDnsResourceRecord::MDnsResourceRecord(const MDnsResourceRecord& other) =
    default;
MDnsResourceRecord::~MDnsResourceRecord() = default;

bool MDnsResourceRecord::Read(rtc::ByteBufferReader* buf) {
  if (!ReadDomainName(buf, &name_)) {
    return false;
  }
  if (!buf->ReadUInt16(&type_) || !buf->ReadUInt16(&class_) ||
      !buf->ReadUInt32(&ttl_seconds_) || !buf->ReadUInt16(&rdlength_)) {
    return false;
  }

  switch (GetType()) {
    case SectionDataType::kA:
      return ReadARData(buf);
    case SectionDataType::kAAAA:
      return ReadQuadARData(buf);
    case SectionDataType::kUnsupported:
      return false;
    default:
      RTC_NOTREACHED();
  }
  return false;
}
bool MDnsResourceRecord::ReadARData(rtc::ByteBufferReader* buf) {
  // A RDATA contains a 32-bit IPv4 address.
  if (!buf->ReadString(&rdata_, 4)) {
    return false;
  }
  return true;
}

bool MDnsResourceRecord::ReadQuadARData(rtc::ByteBufferReader* buf) {
  // AAAA RDATA contains a 128-bit IPv6 address.
  if (!buf->ReadString(&rdata_, 16)) {
    return false;
  }
  return true;
}

bool MDnsResourceRecord::Write(rtc::ByteBufferWriter* buf) const {
  WriteDomainName(buf, name_);
  buf->WriteUInt16(type_);
  buf->WriteUInt16(class_);
  buf->WriteUInt32(ttl_seconds_);
  buf->WriteUInt16(rdlength_);
  switch (GetType()) {
    case SectionDataType::kA:
      WriteARData(buf);
      return true;
    case SectionDataType::kAAAA:
      WriteQuadARData(buf);
      return true;
    case SectionDataType::kUnsupported:
      return false;
    default:
      RTC_NOTREACHED();
  }
  return true;
}

void MDnsResourceRecord::WriteARData(rtc::ByteBufferWriter* buf) const {
  buf->WriteString(rdata_);
}

void MDnsResourceRecord::WriteQuadARData(rtc::ByteBufferWriter* buf) const {
  buf->WriteString(rdata_);
}

void MDnsResourceRecord::SetIPAddressInRecordData(
    const rtc::IPAddress& address) {
  const char* addr;
  if (address.family() == AF_INET) {
    in_addr ip = address.ipv4_address();
    addr = reinterpret_cast<const char*>(&ip);
    rdata_ = std::string(addr, 4);
    rdlength_ = 4;
  } else if (address.family() == AF_INET6) {
    in6_addr ip = address.ipv6_address();
    addr = reinterpret_cast<const char*>(&ip);
    rdata_ = std::string(addr, 16);
    rdlength_ = 16;
  } else {
    RTC_NOTREACHED();
  }
}

bool MDnsResourceRecord::GetIPAddressFromRecordData(
    rtc::IPAddress* address) const {
  if (GetType() != SectionDataType::kA && GetType() != SectionDataType::kAAAA) {
    return false;
  }
  if (rdata_.size() != 4 && rdata_.size() != 16) {
    return false;
  }
  if (rdata_.size() == 4) {
    const in_addr* ipv4 = reinterpret_cast<const in_addr*>(rdata_.data());
    *address = rtc::IPAddress(*ipv4);
  } else if (rdata_.size() == 16) {
    const in6_addr* ipv6 = reinterpret_cast<const in6_addr*>(rdata_.data());
    *address = rtc::IPAddress(*ipv6);
  } else {
    return false;
  }
  return true;
}

MDnsMessage::MDnsMessage() = default;
MDnsMessage::~MDnsMessage() = default;

bool MDnsMessage::Read(rtc::ByteBufferReader* buf) {
  if (!header_.Read(buf)) {
    return false;
  }

  auto read_question = [&buf](std::vector<MDnsQuestion>* section,
                              uint16_t count) {
    section->resize(count);
    for (auto& question : (*section)) {
      if (!question.Read(buf)) {
        return false;
      }
    }
    return true;
  };
  auto read_rr = [&buf](std::vector<MDnsResourceRecord>* section,
                        uint16_t count) {
    section->resize(count);
    for (auto& rr : (*section)) {
      if (!rr.Read(buf)) {
        return false;
      }
    }
    return true;
  };

  if (!read_question(&question_section_, header_.qdcount) ||
      !read_rr(&answer_section_, header_.ancount) ||
      !read_rr(&authority_section_, header_.nscount) ||
      !read_rr(&additional_section_, header_.arcount)) {
    return false;
  }
  return true;
}

bool MDnsMessage::Write(rtc::ByteBufferWriter* buf) const {
  header_.Write(buf);

  auto write_rr = [&buf](const std::vector<MDnsResourceRecord>& section) {
    for (auto rr : section) {
      if (!rr.Write(buf)) {
        return false;
      }
    }
    return true;
  };

  for (auto question : question_section_) {
    question.Write(buf);
  }
  if (!write_rr(answer_section_) || !write_rr(authority_section_) ||
      !write_rr(additional_section_)) {
    return false;
  }

  return true;
}

bool MDnsMessage::ShouldUnicastResponse() const {
  bool should_unicast = false;
  for (const auto& question : question_section_) {
    should_unicast |= question.ShouldUnicastResponse();
  }
  return should_unicast;
}

void MDnsMessage::AddQuestion(const MDnsQuestion& question) {
  question_section_.push_back(question);
  header_.qdcount = question_section_.size();
}

void MDnsMessage::AddAnswerRecord(const MDnsResourceRecord& answer) {
  answer_section_.push_back(answer);
  header_.ancount = answer_section_.size();
}

int MDnsMessage::GetNumSectionData(Section section) const {
  switch (section) {
    case MDnsMessage::Section::kQuestion:
      return GetNumQuestions();
    case MDnsMessage::Section::kAnswer:
      return GetNumAnswerRecords();
    case MDnsMessage::Section::kAuthority:
      return GetNumNameServerRecords();
    case MDnsMessage::Section::kAdditional:
      return GetNumAdditionalRecords();
    default:
      RTC_NOTREACHED();
  }
  return 0;
}

MDnsResourceRecord* MDnsMessage::GetResourceRecord(MDnsMessage::Section section,
                                                   int index) {
  RTC_DCHECK(section != MDnsMessage::Section::kQuestion);
  int num_section_data = GetNumSectionData(section);
  if (index >= num_section_data) {
    RTC_LOG(LS_ERROR) << "Index out of bound.";
    return nullptr;
  }
  switch (section) {
    case MDnsMessage::Section::kAnswer:
      return &answer_section_[index];
    case MDnsMessage::Section::kAuthority:
      return &authority_section_[index];
    case MDnsMessage::Section::kAdditional:
      return &additional_section_[index];
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

MDnsQuestion* MDnsMessage::GetQuestion(int index) {
  if (index >= GetNumQuestions()) {
    RTC_LOG(LS_ERROR) << "Index out of bound.";
    return nullptr;
  }
  return &question_section_[index];
}

MDnsResourceRecord* MDnsMessage::GetAnswerRecord(int index) {
  return GetResourceRecord(MDnsMessage::Section::kAnswer, index);
}
MDnsResourceRecord* MDnsMessage::GetNameServerRecord(int index) {
  return GetResourceRecord(MDnsMessage::Section::kAuthority, index);
}
MDnsResourceRecord* MDnsMessage::GetAdditionalRecord(int index) {
  return GetResourceRecord(MDnsMessage::Section::kAdditional, index);
}

}  // namespace webrtc
