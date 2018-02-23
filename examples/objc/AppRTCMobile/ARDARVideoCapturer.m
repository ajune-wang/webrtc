//
//  ARDARVideoCapturer.m
//  sources
//
//  Created by Anders Carlsson on 2017-12-08.
//

#import "ARDARVideoCapturer.h"

#import "RTCDispatcher+Private.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrameBuffer.h"
#import "SegmentAlgorithms/SegmentAlgorithms.h"

const int64_t kNanosecondsPerSecond = 1000000000;

@implementation ARDARVideoCapturer {
    ARSession *_arSession;
    RTCVideoRotation _rotation;
#if TARGET_OS_IPHONE
    UIDeviceOrientation _orientation;
    SegmentApis* _segmenter;
    int _index;
    CVPixelBufferRef _mask_buffer;
#endif
}

- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate {
    if (self = [super initWithDelegate:delegate]) {
        if (![self setupCaptureSession]) {
            return nil;
        }
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
#if TARGET_OS_IPHONE
        _rotation = RTCVideoRotation_90;
        _orientation = UIDeviceOrientationPortrait;
        [center addObserver:self
                   selector:@selector(deviceOrientationDidChange:)
                       name:UIDeviceOrientationDidChangeNotification
                     object:nil];
#endif
    }
    return self;
}

#if TARGET_OS_IPHONE
- (void)deviceOrientationDidChange:(NSNotification*) notification{
    _orientation = [UIDevice currentDevice].orientation;
    switch(_orientation) {
        case UIDeviceOrientationPortrait:
            _rotation = RTCVideoRotation_90;
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            _rotation = RTCVideoRotation_270;
            break;
        case UIDeviceOrientationLandscapeLeft:
            _rotation = RTCVideoRotation_0;
            break;
        case UIDeviceOrientationLandscapeRight:
            _rotation = RTCVideoRotation_180;
            break;
        default:
            break;
    }
}
#endif

- (void)startCapture {
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     RTCLogInfo("startCapture");
                                     // noop
#if TARGET_OS_IPHONE
                                     [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
#endif
                                 }];
}

- (void)stopCapture {
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     RTCLogInfo("Stop");
                                     // noop
#if TARGET_OS_IPHONE
                                     [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
                                     if (_mask_buffer != NULL)
                                         CVPixelBufferRelease(_mask_buffer);
#endif
                                 }];
}

#pragma mark ARSessionDelegate
- (void)session:(ARSession *)sessionn didUpdateFrame:(ARFrame *)frame {
    NSAssert(frame != nil, @"ARFrame is empty.");
    
    AVDepthData* depth_data = [frame capturedDepthData];
    if (!depth_data) return;
    
    RTCCVPixelBuffer *rtcPixelBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:[frame capturedImage]];
    
    _index++;
    
    CVPixelBufferRef depth_buffer = [depth_data depthDataMap];
    if (_mask_buffer == NULL)
      CVPixelBufferCreate(nil, [rtcPixelBuffer width],[rtcPixelBuffer height], kCVPixelFormatType_OneComponent8, nil, &_mask_buffer);
    
    [_segmenter rgbdSegmentAlpha:frame.capturedImage
                     depthBuffer:depth_buffer
                      maskBuffer:_mask_buffer
                           index:_index
                    snapshotPath:@""];
    
    RTCCVPixelBuffer *rtcDepthBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_mask_buffer];
    
    int64_t timeStampNs = [frame timestamp] * kNanosecondsPerSecond;
    RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                      withDepthBuffer:rtcDepthBuffer
                                                             rotation:_rotation
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
    
    _segmenter =[[SegmentApis alloc] init];
    _index = 0;
    _mask_buffer = NULL;
    
    NSAssert([ARFaceTrackingConfiguration isSupported], @"face tracking unsupported");
    ARFaceTrackingConfiguration *configuration = [[ARFaceTrackingConfiguration alloc] init];
    configuration.lightEstimationEnabled = YES;
    [_arSession runWithConfiguration:configuration
                             options:ARSessionRunOptionResetTracking |
     ARSessionRunOptionRemoveExistingAnchors];
    
    return YES;
}

@end
