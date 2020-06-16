/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_
#define API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_

#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace webrtc {
// Structures to build and parse dependency descriptor as described in
// https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension
class RenderResolution {
 public:
  constexpr RenderResolution() = default;
  constexpr RenderResolution(int width, int height)
      : width_(width), height_(height) {}
  RenderResolution(const RenderResolution&) = default;
  RenderResolution& operator=(const RenderResolution&) = default;

  friend bool operator==(const RenderResolution& lhs,
                         const RenderResolution& rhs) {
    return lhs.width_ == rhs.width_ && lhs.height_ == rhs.height_;
  }

  constexpr int Width() const { return width_; }
  constexpr int Height() const { return height_; }

 private:
  int width_ = 0;
  int height_ = 0;
};

// Relationship of a frame to a Decode target.
enum class DecodeTargetIndication {
  kNotPresent = 0,   // DecodeTargetInfo symbol '-'
  kDiscardable = 1,  // DecodeTargetInfo symbol 'D'
  kSwitch = 2,       // DecodeTargetInfo symbol 'S'
  kRequired = 3      // DecodeTargetInfo symbol 'R'
};

class DecodeTargetIndicationVector {
 public:
  using value_type = DecodeTargetIndication;
  using size_type = size_t;
  class const_iterator {
   public:
    const_iterator(uint64_t data, size_type index)
        : data_(data), index_(index) {}

    friend bool operator==(const const_iterator& lhs,
                           const const_iterator& rhs) {
      // DCHECK_EQ(lhs.data_, rhs.data_);
      return lhs.index_ == rhs.index_;
    }
    friend bool operator!=(const const_iterator& lhs,
                           const const_iterator& rhs) {
      return !(lhs == rhs);
    }

    DecodeTargetIndication operator*() const {
      return DecodeTargetIndicationVector::Get(data_, index_);
    }
    const_iterator& operator++() {
      ++index_;
      return *this;
    }

   private:
    uint64_t data_;
    size_type index_;
  };

  DecodeTargetIndicationVector() = default;
  DecodeTargetIndicationVector(const DecodeTargetIndicationVector&) = default;
  DecodeTargetIndicationVector(
      std::initializer_list<DecodeTargetIndication> values) {
    assign(values.begin(), values.end());
  }
  DecodeTargetIndicationVector& operator=(const DecodeTargetIndicationVector&) =
      default;

  template <typename Iterator>
  void assign(Iterator from, Iterator to) {
    data_ = 0;
    size_ = 0;
    for (; from != to; ++from) {
      data_ |= (static_cast<uint64_t>(*from) << (size_ * 2));
      ++size_;
    }
  }

  // Assign from raw dti representation.
  void Assign(uint64_t data, size_type size) {
    data_ = data;
    size_ = size;
  }
  // Returns raw dti representation.
  uint64_t Data() const { return data_ & ((uint64_t{1} << size_) - 1); }

  friend bool operator==(const DecodeTargetIndicationVector& lhs,
                         const DecodeTargetIndicationVector& rhs) {
    return lhs.size() == rhs.size() && lhs.ActiveData() == rhs.ActiveData();
  }
  friend bool operator!=(const DecodeTargetIndicationVector& lhs,
                         const DecodeTargetIndicationVector& rhs) {
    return !(lhs == rhs);
  }

  const_iterator begin() const { return const_iterator(data_, 0); }
  const_iterator end() const { return const_iterator(data_, size_); }
  value_type operator[](size_type index) const { return Get(data_, index); }

  bool empty() const { return size_ == 0; }
  size_type size() const { return size_; }
  void push_back(DecodeTargetIndication value) {
    // RTC_DCHECK_LT(size_, 32);
    data_ = ActiveData() | (static_cast<uint64_t>(value) << (size_ * 2));
    size_ += 2;
  }

 private:
  static DecodeTargetIndication Get(uint32_t data, size_type index) {
    return static_cast<DecodeTargetIndication>((data >> (index * 2)) & 0b11);
  }

  uint64_t ActiveData() const { return data_ & ((uint64_t{1} << size_) - 1); }
  // up to 32 decode targets are supported.
  uint64_t data_ = 0;
  size_type size_ = 0;
};

struct FrameDependencyTemplate {
  // Setters are named briefly to chain them when building the template.
  FrameDependencyTemplate& S(int spatial_layer);
  FrameDependencyTemplate& T(int temporal_layer);
  FrameDependencyTemplate& Dtis(absl::string_view dtis);
  FrameDependencyTemplate& FrameDiffs(std::initializer_list<int> diffs);
  FrameDependencyTemplate& ChainDiffs(std::initializer_list<int> diffs);

  friend bool operator==(const FrameDependencyTemplate& lhs,
                         const FrameDependencyTemplate& rhs) {
    return lhs.spatial_id == rhs.spatial_id &&
           lhs.temporal_id == rhs.temporal_id &&
           lhs.decode_target_indications == rhs.decode_target_indications &&
           lhs.frame_diffs == rhs.frame_diffs &&
           lhs.chain_diffs == rhs.chain_diffs;
  }

  int spatial_id = 0;
  int temporal_id = 0;
  DecodeTargetIndicationVector decode_target_indications;
  absl::InlinedVector<int, 4> frame_diffs;
  absl::InlinedVector<int, 4> chain_diffs;
};

struct FrameDependencyStructure {
  friend bool operator==(const FrameDependencyStructure& lhs,
                         const FrameDependencyStructure& rhs) {
    return lhs.num_decode_targets == rhs.num_decode_targets &&
           lhs.num_chains == rhs.num_chains &&
           lhs.decode_target_protected_by_chain ==
               rhs.decode_target_protected_by_chain &&
           lhs.resolutions == rhs.resolutions && lhs.templates == rhs.templates;
  }

  int structure_id = 0;
  int num_decode_targets = 0;
  int num_chains = 0;
  // If chains are used (num_chains > 0), maps decode target index into index of
  // the chain protecting that target or |num_chains| value if decode target is
  // not protected by a chain.
  absl::InlinedVector<int, 10> decode_target_protected_by_chain;
  absl::InlinedVector<RenderResolution, 4> resolutions;
  std::vector<FrameDependencyTemplate> templates;
};

struct DependencyDescriptor {
  bool first_packet_in_frame = true;
  bool last_packet_in_frame = true;
  int frame_number = 0;
  FrameDependencyTemplate frame_dependencies;
  absl::optional<RenderResolution> resolution;
  absl::optional<uint32_t> active_decode_targets_bitmask;
  std::unique_ptr<FrameDependencyStructure> attached_structure;
};

// Below are implementation details.
namespace webrtc_impl {
DecodeTargetIndicationVector StringToDecodeTargetIndications(
    absl::string_view indication_symbols);
}  // namespace webrtc_impl

inline FrameDependencyTemplate& FrameDependencyTemplate::S(int spatial_layer) {
  this->spatial_id = spatial_layer;
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::T(int temporal_layer) {
  this->temporal_id = temporal_layer;
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::Dtis(
    absl::string_view dtis) {
  this->decode_target_indications =
      webrtc_impl::StringToDecodeTargetIndications(dtis);
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::FrameDiffs(
    std::initializer_list<int> diffs) {
  this->frame_diffs.assign(diffs.begin(), diffs.end());
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::ChainDiffs(
    std::initializer_list<int> diffs) {
  this->chain_diffs.assign(diffs.begin(), diffs.end());
  return *this;
}

}  // namespace webrtc

#endif  // API_TRANSPORT_RTP_DEPENDENCY_DESCRIPTOR_H_
