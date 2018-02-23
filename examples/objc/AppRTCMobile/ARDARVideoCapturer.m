//
//  ARDARVideoCapturer.m
//  sources
//
//  Created by Anders Carlsson on 2017-12-08.
//

#import "ARDARVideoCapturer.h"

#import <SegmentAlgorithms/SegmentAlgorithms.h>
#import "RTCDispatcher+Private.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

const int64_t kNanosecondsPerSecond = 1000000000;

@implementation ARDARVideoCapturer {
  ARSession *_arSession;
  SegmentApis *_segmenter;
  int _index;
}

- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate {
  if (self = [super initWithDelegate:delegate]) {
    if (![self setupCaptureSession]) {
      return nil;
    }
  }
  return self;
}

- (void)startCapture {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                 RTCLogInfo("startCapture");
                                 // noop
                               }];
}

- (void)stopCapture {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                 RTCLogInfo("Stop");
                                 // noop
                               }];
}

#pragma mark ARSessionDelegate
- (void)session:(ARSession *)sessionn didUpdateFrame:(ARFrame *)frame {
  NSAssert(frame != nil, @"ARFrame is empty.");

  if (frame.capturedDepthData == nil) return;

  [_segmenter rgbdSegmentPixelBuffers:frame.capturedImage
                          depthBuffer:frame.capturedDepthData.depthDataMap
                                index:_index
                         snapshotPath:@""];
  _index = _index + 1;

  RTCCVPixelBuffer *rtcPixelBuffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:[frame capturedImage]];
  int64_t timeStampNs = [frame timestamp] * kNanosecondsPerSecond;
  RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                           rotation:RTCVideoRotation_90
                                                        timeStampNs:timeStampNs];
  [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
}

#pragma mark - ARSession notifications

- (void)sessionWasInterruption:(ARSession *)session {
  RTCLog(@"ARSession interrupted");
}

- (void)sInterruptionEnded:(ARSession *)session {
  RTCLog(@"ARSession interruption ended.");
}

- (void)session:(ARSession *)session didFailWithError:(NSError *)error {
  RTCLogError(@"ARSession error: %@", error);
}

#pragma mark - Private

- (BOOL)setupCaptureSession {
  NSAssert(_arSession == nil, @"Setup session called twice.");
  _arSession = [[ARSession alloc] init];
  _arSession.delegate = self;

  _segmenter = [[SegmentApis alloc] init];
  _index = 0;

  NSAssert([ARFaceTrackingConfiguration isSupported], @"face tracking unsupported");
  ARFaceTrackingConfiguration *configuration = [[ARFaceTrackingConfiguration alloc] init];
  configuration.lightEstimationEnabled = YES;
  [_arSession runWithConfiguration:configuration
                           options:ARSessionRunOptionResetTracking |
                           ARSessionRunOptionRemoveExistingAnchors];

  return YES;
}

@end
