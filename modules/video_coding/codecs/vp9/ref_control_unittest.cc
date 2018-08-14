/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "modules/video_coding/codecs/vp9/ref_control.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const size_t kSpatialRefBufIdx = 7;
}  // namespace

TEST(TestVp9ReferenceControl, SetFrameReferences1SL1TL) {
  std::vector<ReferenceControl::ReferenceConfig> layers;
  layers.push_back({1, 0ull, false, {0}, {true}, {0}, {0}});

  vpx_svc_ref_frame_config_t ref_cfg;
  for (size_t pics_since_key = 0; pics_since_key < 4; ++pics_since_key) {
    ref_cfg = ReferenceControl::SetFrameReferences(layers);

    EXPECT_EQ(ref_cfg.lst_fb_idx[0], 0);
    EXPECT_EQ(ref_cfg.gld_fb_idx[0], 0);
    EXPECT_EQ(ref_cfg.alt_fb_idx[0], 0);
    EXPECT_EQ(ref_cfg.update_buffer_slot[0], layers[0].upd_buf_idx[0]);

    ++layers[0].frames_since_key;
  }
}

TEST(Vp9RefControl, SetFrameReferences1SL2TL) {
  std::vector<ReferenceControl::ReferenceConfig> layers;
  layers.push_back({2, 0ull, false, {0, 1}, {true, false}, {0, 0}, {0, 0}});

  vpx_svc_ref_frame_config_t ref_cfg;
  for (int pics_since_key = 0; pics_since_key < 4; ++pics_since_key) {
    const size_t gof_idx = layers[0].frames_since_key % layers[0].gof_size;
    ref_cfg = ReferenceControl::SetFrameReferences(layers);

    EXPECT_EQ(ref_cfg.lst_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);
    EXPECT_EQ(ref_cfg.gld_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);
    EXPECT_EQ(ref_cfg.alt_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);

    if (layers[0].ref_frame_flag[gof_idx]) {
      EXPECT_EQ(ref_cfg.update_buffer_slot[0],
                1 << layers[0].upd_buf_idx[gof_idx]);
    } else {
      EXPECT_EQ(ref_cfg.update_buffer_slot[0], 0);
    }

    for (ReferenceControl::ReferenceConfig& layer : layers) {
      ++layer.frames_since_key;
    }
  }
}

TEST(Vp9RefControl, SetFrameReferences1SL3TL) {
  std::vector<ReferenceControl::ReferenceConfig> layers;
  layers.push_back({4,
                    0ull,
                    false,
                    {0, 2, 1, 2},
                    {true, false, true, false},
                    {0, 0, 0, 1},
                    {0, 0, 1, 0}});

  vpx_svc_ref_frame_config_t ref_cfg;
  for (int pics_since_key = 0; pics_since_key < 4; ++pics_since_key) {
    const size_t gof_idx = layers[0].frames_since_key % layers[0].gof_size;
    ref_cfg = ReferenceControl::SetFrameReferences(layers);

    EXPECT_EQ(ref_cfg.lst_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);
    EXPECT_EQ(ref_cfg.gld_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);
    EXPECT_EQ(ref_cfg.alt_fb_idx[0], layers[0].ref_buf_idx[gof_idx]);

    if (layers[0].ref_frame_flag[gof_idx]) {
      EXPECT_EQ(ref_cfg.update_buffer_slot[0],
                1 << layers[0].upd_buf_idx[gof_idx]);
    } else {
      EXPECT_EQ(ref_cfg.update_buffer_slot[0], 0);
    }

    for (ReferenceControl::ReferenceConfig& layer : layers) {
      ++layer.frames_since_key;
    }
  }
}

TEST(Vp9RefControl, SetFrameReferences3SL3TL) {
  std::vector<ReferenceControl::ReferenceConfig> layers;
  layers.push_back({4,
                    0ull,
                    false,
                    {0, 2, 1, 2},
                    {true, false, true, false},
                    {0, 0, 0, 1},
                    {0, 0, 1, 0}});

  layers.push_back({4,
                    1ull,
                    true,
                    {0, 2, 1, 2},
                    {true, false, true, false},
                    {2, 2, 2, 3},
                    {2, 0, 3, 0}});

  layers.push_back({4,
                    2ull,
                    true,
                    {0, 2, 1, 2},
                    {true, false, true, false},
                    {4, 4, 4, 5},
                    {4, 0, 5, 0}});

  vpx_svc_ref_frame_config_t ref_cfg;
  for (int pics_since_key = 0; pics_since_key < 4; ++pics_since_key) {
    const size_t gof_idx = layers[0].frames_since_key % layers[0].gof_size;
    ref_cfg = ReferenceControl::SetFrameReferences(layers);

    for (size_t i = 0; i < layers.size(); ++i) {
      if (layers[i].frames_since_key > 0) {
        EXPECT_EQ(ref_cfg.lst_fb_idx[i], layers[i].ref_buf_idx[gof_idx]);
        EXPECT_EQ(ref_cfg.alt_fb_idx[i], layers[i].ref_buf_idx[gof_idx]);
      } else if (i > 0) {
        const int base_ref_idx = ffs(ref_cfg.update_buffer_slot[i - 1]) - 1;
        EXPECT_EQ(ref_cfg.lst_fb_idx[i], base_ref_idx);
        EXPECT_EQ(ref_cfg.alt_fb_idx[i], base_ref_idx);
      }

      if (layers[i].inter_layer_pred) {
        const int base_ref_idx = ffs(ref_cfg.update_buffer_slot[i - 1]) - 1;
        EXPECT_EQ(ref_cfg.gld_fb_idx[i], base_ref_idx);
      } else {
        EXPECT_EQ(ref_cfg.gld_fb_idx[i], layers[i].ref_buf_idx[gof_idx]);
      }

      if (layers[i].ref_frame_flag[gof_idx]) {
        EXPECT_EQ(ref_cfg.update_buffer_slot[i],
                  1 << layers[i].upd_buf_idx[gof_idx]);
      } else if (i + 1 < layers.size() && layers[i + 1].inter_layer_pred) {
        EXPECT_EQ(ref_cfg.update_buffer_slot[i], 1 << kSpatialRefBufIdx);
      }

      ++layers[i].frames_since_key;
    }
  }
}

}  // namespace webrtc
