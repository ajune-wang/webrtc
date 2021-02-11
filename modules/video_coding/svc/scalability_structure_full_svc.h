/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_FULL_SVC_H_
#define MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_FULL_SVC_H_

#include <bitset>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

namespace webrtc {

class ScalabilityStructureFullSvc : public ScalableVideoController {
 public:
  struct Settings {
    int num_spatial_layers = 1;
    int num_temporal_layers = 1;
    int scaling_factor_num = 1;
    int scaling_factor_den = 2;
  };
  explicit ScalabilityStructureFullSvc(Settings settings);
  ~ScalabilityStructureFullSvc() override;

  StreamLayersConfig StreamConfig() const override;

  std::vector<LayerFrameConfig> NextFrameConfig(bool restart) override;
  GenericFrameInfo OnEncodeDone(const LayerFrameConfig& config) override;
  void OnRatesUpdated(const VideoBitrateAllocation& bitrates) override;

 protected:
  static FrameDependencyStructure L1T2();
  static FrameDependencyStructure L1T3();
  static FrameDependencyStructure L2T1();
  static FrameDependencyStructure L2T2();
  static FrameDependencyStructure L3T1();
  static FrameDependencyStructure L3T3();

 private:
  enum FramePattern {
    kNone,
    kDeltaT2A,
    kDeltaT1,
    kDeltaT2B,
    kDeltaT0,
  };
  static constexpr absl::string_view kFramePatternNames[] = {
      "None", "DeltaT2A", "DeltaT1", "DeltaT2B", "DeltaT0"};
  static constexpr int kMaxNumSpatialLayers = 3;
  static constexpr int kMaxNumTemporalLayers = 3;

  // Index of the buffer to store last frame for layer (`sid`, `tid`)
  int BufferIndex(int sid, int tid) const {
    return tid * settings_.num_spatial_layers + sid;
  }
  bool DecodeTargetIsActive(int sid, int tid) const {
    return active_decode_targets_[sid * settings_.num_temporal_layers + tid];
  }
  void SetDecodeTargetIsActive(int sid, int tid, bool value) {
    active_decode_targets_.set(sid * settings_.num_temporal_layers + tid,
                               value);
  }
  FramePattern NextPattern() const;
  bool TemporalLayerIsActive(int tid) const;
  static DecodeTargetIndication Dti(int sid,
                                    int tid,
                                    const LayerFrameConfig& frame);

  const Settings settings_;

  FramePattern last_pattern_ = kNone;
  std::bitset<kMaxNumSpatialLayers> can_reference_t0_frame_for_spatial_id_ = 0;
  std::bitset<kMaxNumSpatialLayers> can_reference_t1_frame_for_spatial_id_ = 0;
  std::bitset<32> active_decode_targets_;
};

class ScalabilityStructureL1T2 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL1T2()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 1;
          settings.num_temporal_layers = 2;
          return settings;
        }()) {}
  ~ScalabilityStructureL1T2() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L1T2();
  }
};

class ScalabilityStructureL1T2h : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL1T2h()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 1;
          settings.num_temporal_layers = 2;
          settings.scaling_factor_num = 2;
          settings.scaling_factor_den = 3;
          return settings;
        }()) {}
  ~ScalabilityStructureL1T2h() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L1T2();
  }
};

// T2       0   0   0   0
//          |  /    |  /
// T1       / 0     / 0  ...
//         |_/     |_/
// T0     0-------0------
// Time-> 0 1 2 3 4 5 6 7
class ScalabilityStructureL1T3 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL1T3()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 1;
          settings.num_temporal_layers = 3;
          return settings;
        }()) {}
  ~ScalabilityStructureL1T3() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L1T3();
  }
};

// S1  0--0--0-
//     |  |  | ...
// S0  0--0--0-
class ScalabilityStructureL2T1 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL2T1()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 2;
          settings.num_temporal_layers = 1;
          return settings;
        }()) {}
  ~ScalabilityStructureL2T1() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L2T1();
  }
};

class ScalabilityStructureL2T1h : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL2T1h()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 2;
          settings.num_temporal_layers = 1;
          settings.scaling_factor_num = 2;
          settings.scaling_factor_den = 3;
          return settings;
        }()) {}
  ~ScalabilityStructureL2T1h() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L2T1();
  }
};

// S1T1     0   0
//         /|  /|  /
// S1T0   0-+-0-+-0
//        | | | | | ...
// S0T1   | 0 | 0 |
//        |/  |/  |/
// S0T0   0---0---0--
// Time-> 0 1 2 3 4
class ScalabilityStructureL2T2 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL2T2()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 2;
          settings.num_temporal_layers = 2;
          return settings;
        }()) {}
  ~ScalabilityStructureL2T2() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L2T2();
  }
};

// S2     0-0-0-
//        | | |
// S1     0-0-0-...
//        | | |
// S0     0-0-0-
// Time-> 0 1 2
class ScalabilityStructureL3T1 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL3T1()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 3;
          settings.num_temporal_layers = 1;
          return settings;
        }()) {}
  ~ScalabilityStructureL3T1() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L3T1();
  }
};

// https://aomediacodec.github.io/av1-rtp-spec/#a1022-l3t3-full-svc
class ScalabilityStructureL3T3 : public ScalabilityStructureFullSvc {
 public:
  ScalabilityStructureL3T3()
      : ScalabilityStructureFullSvc([] {
          Settings settings;
          settings.num_spatial_layers = 3;
          settings.num_temporal_layers = 3;
          return settings;
        }()) {}
  ~ScalabilityStructureL3T3() override = default;

  FrameDependencyStructure DependencyStructure() const override {
    return L3T3();
  }
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_FULL_SVC_H_
