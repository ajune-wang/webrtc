/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioSession+Private.h"
#import "RTCAudioSessionConfiguration.h"

#import "base/RTCLogging.h"

@implementation RTCAudioSession (Configuration)

- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                   error:(NSError **)outError {
  return [self setConfiguration:configuration
                         active:NO
                shouldSetActive:NO
                          error:outError];
}

- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                  active:(BOOL)active
                   error:(NSError **)outError {
  return [self setConfiguration:configuration
                         active:active
                shouldSetActive:YES
                          error:outError];
}

#pragma mark - Private

- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                  active:(BOOL)active
         shouldSetActive:(BOOL)shouldSetActive
                   error:(NSError **)outError {
  NSParameterAssert(configuration);
  if (outError) {
    *outError = nil;
  }
  if (![self checkLock:outError]) {
    return NO;
  }

  // Provide an error even if there isn't one so we can log it. We will not
  // return immediately on error in this function and instead try to set
  // everything we can.
  NSError *error = nil;

  if (self.category != configuration.category ||
      self.categoryOptions != configuration.categoryOptions) {
    NSError *categoryError = nil;
    if (![self setCategory:configuration.category
               withOptions:configuration.categoryOptions
                     error:&categoryError]) {
      RTCLogError(@"Failed to set category: %@",
                  categoryError.localizedDescription);
      error = categoryError;
    } else {
      RTCLog(@"Set category to: %@", configuration.category);
    }
  }

  if (self.mode != configuration.mode) {
    NSError *modeError = nil;
    if (![self setMode:configuration.mode error:&modeError]) {
      RTCLogError(@"Failed to set mode: %@",
                  modeError.localizedDescription);
      error = modeError;
    } else {
      RTCLog(@"Set mode to: %@", configuration.mode);
    }
  }

  // Sometimes category options don't stick after setting mode.
  if (self.categoryOptions != configuration.categoryOptions) {
    NSError *categoryError = nil;
    if (![self setCategory:configuration.category
               withOptions:configuration.categoryOptions
                     error:&categoryError]) {
      RTCLogError(@"Failed to set category options: %@",
                  categoryError.localizedDescription);
      error = categoryError;
    } else {
      RTCLog(@"Set category options to: %ld",
             (long)configuration.categoryOptions);
    }
  }

  if (self.preferredSampleRate != configuration.sampleRate) {
    [self setPreferredSampleRate:configuration.sampleRate error:&error];
  }

  if (self.preferredIOBufferDuration != configuration.ioBufferDuration) {
    [self setPreferredIOBufferDuration:configuration.ioBufferDuration error:&error];
  }

  if (shouldSetActive) {
    NSError *activeError = nil;
    if (![self setActive:active error:&activeError]) {
      RTCLogError(@"Failed to setActive to %d: %@",
                  active, activeError.localizedDescription);
      error = activeError;
    }
  }

  if (self.isActive &&
      // TODO(tkchin): Figure out which category/mode numChannels is valid for.
      [self.mode isEqualToString:AVAudioSessionModeVoiceChat]) {
    // Try to set the preferred number of hardware audio channels. These calls
    // must be done after setting the audio session’s category and mode and
    // activating the session.
    NSInteger inputNumberOfChannels = configuration.inputNumberOfChannels;
    if (self.inputNumberOfChannels != inputNumberOfChannels) {
      [self setPreferredInputNumberOfChannels:inputNumberOfChannels error:&error];
    }
    NSInteger outputNumberOfChannels = configuration.outputNumberOfChannels;
    if (self.outputNumberOfChannels != outputNumberOfChannels) {
      [self setPreferredOutputNumberOfChannels:outputNumberOfChannels error:&error];
    }
  }

  if (outError) {
    *outError = error;
  }

  return error == nil;
}

@end
