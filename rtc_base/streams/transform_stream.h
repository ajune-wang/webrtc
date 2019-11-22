/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TRANSFORM_STREAM_H_
#define RTC_BASE_STREAMS_TRANSFORM_STREAM_H_

#include <memory>
#include <utility>

#include "rtc_base/streams/readable_stream.h"
#include "rtc_base/streams/underlying_transformer.h"
#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

class TransformStreamBase {
 public:
  virtual ~TransformStreamBase() = default;

  virtual WritableStreamBase* writable() = 0;
  virtual ReadableStreamBase* readable() = 0;
};

template <typename I>
class TransformInputController {
 public:
  virtual ~TransformInputController() = default;

  virtual void OnStart(WritableStreamController<I>*) = 0;
  virtual void OnWrite(I chunk) = 0;
  virtual void OnClose() = 0;
};

template <typename I>
class TransformStreamSink : public UnderlyingSink<I> {
 public:
  explicit TransformStreamSink(TransformInputController<I>* input_controller)
      : input_controller_(input_controller) {
    RTC_DCHECK(input_controller_);
  }

  // UnderlyingSink implementation.
  void Start(WritableStreamController<I>* controller) override {
    input_controller_->OnStart(controller);
  }
  void Write(I chunk, WritableStreamController<I>*) override {
    input_controller_->OnWrite(std::move(chunk));
  }
  void Close(WritableStreamController<I>*) override {
    input_controller_->OnClose();
  }

 private:
  TransformInputController<I>* input_controller_;
};

template <typename O>
class TransformOutputController {
 public:
  virtual ~TransformOutputController() = default;

  virtual void OnStart(ReadableStreamController<O>*) = 0;
  virtual void OnPull() = 0;
};

template <typename O>
class TransformStreamSource : public UnderlyingSource<O> {
 public:
  explicit TransformStreamSource(
      TransformOutputController<O>* output_controller)
      : output_controller_(output_controller) {
    RTC_DCHECK(output_controller_);
  }

  // UnderlyingSource implementation.
  void Start(ReadableStreamController<O>* controller) override {
    output_controller_->OnStart(controller);
  }
  void Pull(ReadableStreamController<O>* controller) override {
    output_controller_->OnPull();
  }

 private:
  TransformOutputController<O>* output_controller_;
};

template <typename I, typename O>
class TransformStream final : public TransformStreamBase,
                              private TransformInputController<I>,
                              private TransformOutputController<O>,
                              private TransformStreamController<O> {
 public:
  explicit TransformStream(
      std::unique_ptr<UnderlyingTransformer<I, O>> transformer)
      : transformer_(std::move(transformer)),
        writable_(std::make_unique<TransformStreamSink<I>>(
            static_cast<TransformInputController<I>*>(this))),
        readable_(std::make_unique<TransformStreamSource<O>>(
            static_cast<TransformOutputController<O>*>(this))) {
    RTC_DCHECK(transformer_);
    RTC_DCHECK(writable_controller_);
    RTC_DCHECK(readable_controller_);
    Start();
  }

  WritableStream<I>* writable() { return &writable_; }
  ReadableStream<O>* readable() { return &readable_; }

 private:
  void Start() {
    RTC_DCHECK(state_ == State::kInit);
    state_ = State::kStarting;
    transformer_->Start(this);
    if (state_ != State::kStarting) {
      return;
    }
    state_ = State::kBlocked;
    readable_controller_->CompleteAsync();
  }

  // TransformInputController implementation.
  void OnStart(WritableStreamController<I>* writable_controller) override {
    writable_controller_ = writable_controller;
    writable_controller_->StartAsync();
  }
  void OnWrite(I chunk) override {
    RTC_DCHECK(state_ == State::kReady);
    state_ = State::kTransforming;
    transformer_->Transform(std::move(chunk), this);
    if (state_ != State::kTransforming) {
      return;
    }
    if (!readable_controller_->IsWritable()) {
      state_ = State::kBlocked;
      writable_controller_->StartAsync();
      return;
    }
    state_ = State::kReady;
  }
  void OnClose() override {
    RTC_DCHECK(state_ == State::kReady);
    state_ = State::kClosing;
    transformer_->Close(this);
    if (state_ != State::kClosing) {
      return;
    }
    state_ = State::kClosed;
    readable_controller_->Close();
  }

  // TransformOutputController implementation.
  void OnStart(ReadableStreamController<O>* readable_controller) override {
    readable_controller_ = readable_controller;
    readable_controller_->StartAsync();
  }
  void OnPull() override {
    switch (state_) {
      case State::kReady:
        return;
      case State::kBlocked:
        state_ = State::kReady;
        writable_controller_->CompleteAsync();
        return;
      case State::kTransformPending:
        transformer_->Flush(this);
        return;
      default:
        RTC_NOTREACHED();
    }
  }

  // TransformStreamController implementation.
  bool IsWritable() override {
    return readable_controller_ && readable_controller_->IsWritable();
  }
  void Write(O chunk) override {
    if (!readable_controller_) {
      return;
    }
    readable_controller_->Write(std::move(chunk));
  }
  void StartAsync() override {
    switch (state_) {
      case State::kStarting:
        state_ = State::kStartPending;
        readable_controller_->StartAsync();
        break;
      case State::kTransforming:
        state_ = State::kTransformPending;
        writable_controller_->StartAsync();
        break;
      case State::kClosing:
        state_ = State::kClosePending;
        break;
      default:
        RTC_NOTREACHED();
    }
  }
  void CompleteAsync() override {
    switch (state_) {
      case State::kStartPending:
        state_ = State::kBlocked;
        readable_controller_->CompleteAsync();
        break;
      case State::kTransformPending:
        if (!readable_controller_->IsWritable()) {
          state_ = State::kBlocked;
          return;
        }
        state_ = State::kReady;
        writable_controller_->CompleteAsync();
        break;
      case State::kClosePending:
        state_ = State::kClosed;
        readable_controller_->Close();
        writable_controller_->CompleteAsync();
        break;
      default:
        RTC_NOTREACHED();
    }
  }

  enum class State {
    kInit,
    kStarting,
    kStartPending,
    kReady,
    kTransforming,
    kTransformPending,
    kBlocked,
    kClosing,
    kClosePending,
    kClosed,
  };

  State state_ = State::kInit;
  std::unique_ptr<UnderlyingTransformer<I, O>> transformer_;
  WritableStream<I> writable_;
  WritableStreamController<I>* writable_controller_;
  ReadableStream<O> readable_;
  ReadableStreamController<O>* readable_controller_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TRANSFORM_STREAM_H_
