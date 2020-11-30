/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_seq_num_only_ref_finder.h"

#include <utility>

#include "rtc_base/logging.h"

namespace webrtc {
namespace video_coding {

RtpFrameReferenceFinder::ReturnVector RtpSeqNumOnlyRefFinder::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  FrameDecision decision = ManageFrameInternal(frame.get());

  RtpFrameReferenceFinder::ReturnVector res;
  switch (decision) {
    case kStash:
      if (stashed_frames_.size() > kMaxStashedFrames)
        stashed_frames_.pop_back();
      stashed_frames_.push_front(std::move(frame));
      return res;
    case kHandOff:
      res.push_back(std::move(frame));
      RetryStashedFrames(res);
      return res;
    case kDrop:
      return res;
  }

  return res;
}

// 检查输入帧的连续性，并且设置其参考帧
RtpSeqNumOnlyRefFinder::FrameDecision
RtpSeqNumOnlyRefFinder::ManageFrameInternal(RtpFrameObject* frame) {
  // 如果是关键帧，插入GOP表，{last_seq_num, 初始value{last_seq_num, last_seq_num}
  if (frame->frame_type() == VideoFrameType::kVideoFrameKey) {
    last_seq_num_gop_.insert(std::make_pair(
        frame->last_seq_num(),
        std::make_pair(frame->last_seq_num(), frame->last_seq_num())));
  }

  // 如果GOP表空，就不可能找到参考帧，先缓存
  // We have received a frame but not yet a keyframe, stash this frame.
  if (last_seq_num_gop_.empty())
    return kStash;

  // 删除较老的关键帧(PID小于last_seq_num - 100), 但至少保留一个帧
  // Clean up info for old keyframes but make sure to keep info
  // for the last keyframe.
  auto clean_to = last_seq_num_gop_.lower_bound(frame->last_seq_num() - 100);
  for (auto it = last_seq_num_gop_.begin();
       it != clean_to && last_seq_num_gop_.size() > 1;) {
    it = last_seq_num_gop_.erase(it);
  }

  // 在GOP表中搜索第一个比当前帧新的关键帧
  // Find the last sequence number of the last frame for the keyframe
  // that this frame indirectly references.
  auto seq_num_it = last_seq_num_gop_.upper_bound(frame->last_seq_num());
  // 如果搜索到的关键帧是最老的，说明当前帧比最老的关键帧还老，无法设置参考帧，丢弃
  if (seq_num_it == last_seq_num_gop_.begin()) {
    RTC_LOG(LS_WARNING) << "Generic frame with packet range ["
                        << frame->first_seq_num() << ", "
                        << frame->last_seq_num()
                        << "] has no GoP, dropping frame.";
    return kDrop;
  }
  // 如果搜索到的关键帧不是最老的，那么搜索到的关键帧的上一个关键帧所在的GOP里应该可以找到参考帧，
  // 如果当前帧是关键帧，seq_num_it为end(), seq_num_it--则为最后一个关键帧
  seq_num_it--;

  // 保证帧的连续，不连续则先缓存
  // 当前GOP内最新一个帧的最后一个包的序列号，用于设置为下一个帧的参考帧
  // Make sure the packet sequence numbers are continuous, otherwise stash
  // this frame.
  uint16_t last_picture_id_gop = seq_num_it->second.first;
  // 当前GOP内最新一个包的序列号，有可能是last_picture_id_gop，也有可能是填充包，用于检查帧的连续性
  uint16_t last_picture_id_with_padding_gop = seq_num_it->second.second;
  // P帧的连续性检查
  if (frame->frame_type() == VideoFrameType::kVideoFrameDelta) {
    // 获得P帧第一个包的上个包的序列号, 即上一个帧的最后一个包
    uint16_t prev_seq_num = frame->first_seq_num() - 1;

    // 如果P帧第一个包的上个包的序列号与当前GOP的最新包的序列号不等，说明不连续，先缓存
    if (prev_seq_num != last_picture_id_with_padding_gop)
      return kStash;
  }

  // 现在这个帧是连续的了
  RTC_DCHECK(AheadOrAt(frame->last_seq_num(), seq_num_it->first));

  // 获得当前帧的最后一个包的序列号，设置为初始PID，后面还会设置一次Unwrap
  // Since keyframes can cause reordering we can't simply assign the
  // picture id according to some incrementing counter.
  frame->id.picture_id = frame->last_seq_num();
  // 设置帧的参考帧数，P帧才需要1个参考帧
  frame->num_references =
      frame->frame_type() == VideoFrameType::kVideoFrameDelta;
  // 设置参考帧为当前GOP的最新一个帧的最后一个包的序列号，
  // 既然该帧是连续的，那么其参考帧自然也就是上个帧
  frame->references[0] = rtp_seq_num_unwrapper_.Unwrap(last_picture_id_gop);
  // 如果当前帧比当前GOP的最新一个帧的最后一个包还新，则更新GOP的最新一个帧的最后一个包(first)
  // 以及GOP的最新包(second)
  if (AheadOf<uint16_t>(frame->id.picture_id, last_picture_id_gop)) {
    seq_num_it->second.first = frame->id.picture_id;  // 更新GOP的最新一个帧的最后一个包
    seq_num_it->second.second = frame->id.picture_id; // 更新GOP的最新包，可能被填充包更新
  }

  // 更新填充包状态
  UpdateLastPictureIdWithPadding(frame->id.picture_id);
  // 设置当前帧的PID为Unwrap形式
  frame->id.picture_id = rtp_seq_num_unwrapper_.Unwrap(frame->id.picture_id);
  // 该包已经设置了参考帧且连续，可以向后传递了
  return kHandOff;
}

// 有两种情况可以尝试处理缓存的帧，持续的输出带参考帧的连续的帧
//   1.在输出完一个连续的带参考帧的帧后，帧缓存stashed_frames_中可能还可以输出下一个连续的带参考帧的帧
//   2.收到一个乱序的填充包，导致GOP中的某个P帧连续
void RtpSeqNumOnlyRefFinder::RetryStashedFrames(
    RtpFrameReferenceFinder::ReturnVector& res) {
  bool complete_frame = false;
  do {
    complete_frame = false;
    for (auto frame_it = stashed_frames_.begin();
         frame_it != stashed_frames_.end();) {
      // 处理一个缓存帧，检查是否可以输出带参考帧的连续的帧
      FrameDecision decision = ManageFrameInternal(frame_it->get());

      switch (decision) {
        case kStash:  // 仍然不连续，或者没有参考帧
          ++frame_it; // 检查下一个缓存帧
          break;
        case kHandOff: // 找到了一个带参考帧的连续的帧
          complete_frame = true;
          res.push_back(std::move(*frame_it)); // 多帧输出
          ABSL_FALLTHROUGH_INTENDED;
        case kDrop: // 无论kHandOff、kDrop都可以从缓存中删除了
          frame_it = stashed_frames_.erase(frame_it); // 删除并检查下一个缓存帧
      }
    }
  } while (complete_frame); // 如果能持续找到带参考帧的连续的帧则继续
}

// 检查填充包缓存中的填充包，如果在GOP内连续则更新GOP表的last_picture_id_with_padding_gop字段，
// 保证GOP的最新包序列号为最新的填充包序列号，以保证帧的连续性检查能够正确运行下去
void RtpSeqNumOnlyRefFinder::UpdateLastPictureIdWithPadding(uint16_t seq_num) {
  auto gop_seq_num_it = last_seq_num_gop_.upper_bound(seq_num);

  // If this padding packet "belongs" to a group of pictures that we don't track
  // anymore, do nothing.
  if (gop_seq_num_it == last_seq_num_gop_.begin())
    return;
  --gop_seq_num_it;

  // Calculate the next contiuous sequence number and search for it in
  // the padding packets we have stashed.
  uint16_t next_seq_num_with_padding = gop_seq_num_it->second.second + 1;
  auto padding_seq_num_it =
      stashed_padding_.lower_bound(next_seq_num_with_padding);

  // While there still are padding packets and those padding packets are
  // continuous, then advance the "last-picture-id-with-padding" and remove
  // the stashed padding packet.
  while (padding_seq_num_it != stashed_padding_.end() &&
         *padding_seq_num_it == next_seq_num_with_padding) {
    gop_seq_num_it->second.second = next_seq_num_with_padding;
    ++next_seq_num_with_padding;
    padding_seq_num_it = stashed_padding_.erase(padding_seq_num_it);
  }

  // In the case where the stream has been continuous without any new keyframes
  // for a while there is a risk that new frames will appear to be older than
  // the keyframe they belong to due to wrapping sequence number. In order
  // to prevent this we advance the picture id of the keyframe every so often.
  if (ForwardDiff(gop_seq_num_it->first, seq_num) > 10000) {
    auto save = gop_seq_num_it->second;
    last_seq_num_gop_.clear();
    last_seq_num_gop_[seq_num] = save;
  }
}

RtpFrameReferenceFinder::ReturnVector RtpSeqNumOnlyRefFinder::PaddingReceived(
    uint16_t seq_num) {
  // 只保留最近kMaxPaddingAge个填充包
  // 5 ... 210 -> clean_padding_to(110)
  // 5 ... 90  -> clean_padding_to(5)
  auto clean_padding_to =
      stashed_padding_.lower_bound(seq_num - kMaxPaddingAge);
  stashed_padding_.erase(stashed_padding_.begin(), clean_padding_to);
  // 缓存填充包
  stashed_padding_.insert(seq_num);
  // 更新填充包状态
  UpdateLastPictureIdWithPadding(seq_num);
  RtpFrameReferenceFinder::ReturnVector res;
  // 尝试处理一次缓存的P帧，有可能序列号连续了
  RetryStashedFrames(res);
  return res;
}

void RtpSeqNumOnlyRefFinder::ClearTo(uint16_t seq_num) {
  auto it = stashed_frames_.begin();
  while (it != stashed_frames_.end()) {
     // 如果stash帧的first_seq_num比seq_num老则清除
    if (AheadOf<uint16_t>(seq_num, (*it)->first_seq_num())) {
      it = stashed_frames_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace video_coding
}  // namespace webrtc
