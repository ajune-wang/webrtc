/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "ARDARVideoCapturer.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#import "SegmentAlgorithms/SegmentAlgorithms.h"
#if TARGET_OS_IPHONE
#import "WebRTC/UIDevice+RTCDevice.h"
#endif

#import "AVCaptureSession+DevicePosition.h"
#import "RTCDispatcher+Private.h"

#define kNumBuffer 4
const int64_t kNanosecondsPerSecond = 1000000000;

@interface ARDARVideoCapturer ()<AVCaptureDataOutputSynchronizerDelegate>
@property(nonatomic, readonly) dispatch_queue_t frameQueue;
@end
@interface ARDARVideoCapturer ()<AVCaptureDepthDataOutputDelegate>
@end
@interface ARDARVideoCapturer ()<AVCaptureVideoDataOutputSampleBufferDelegate>
@end
@implementation ARDARVideoCapturer {
    AVCaptureVideoDataOutput *_videoDataOutput;
    AVCaptureDepthDataOutput *_depthDataOutput;
    AVCaptureDataOutputSynchronizer *_syncOutput;

    AVCaptureSession *_captureSession;
    AVCaptureDevice *_currentDevice;
    FourCharCode _preferredOutputPixelFormat;
    FourCharCode _outputPixelFormat;
    BOOL _hasRetriedOnFatalError;
    BOOL _isRunning;
    // Will the session be running once all asynchronous operations have been completed?
    BOOL _willBeRunning;
    RTCVideoRotation _rotation;
#if TARGET_OS_IPHONE
    UIDeviceOrientation _orientation;
#endif
    SegmentApis* _segmenter;
    int _index;
    bool _receive_first_depth;

    CVPixelBufferRef _last_depth_buffer; // Only accessible on Main thread
    CVPixelBufferRef _last_color_buffer; // Only accessible on Main thread

    CVPixelBufferRef _mask_buffer_queue[kNumBuffer];
    CVPixelBufferRef _out_color_buffer_queue[kNumBuffer];

    CVPixelBufferRef _depth_buffer_queue[kNumBuffer];
    CVPixelBufferRef _color_buffer_queue[kNumBuffer];
    CVPixelBufferRef _pre_color_buffer_queue[kNumBuffer];
    bool _buffer_queue_mark[kNumBuffer]; // True means the buffer is free
    dispatch_semaphore_t _buffer_queue_lock;

    NSProcessInfoThermalState _history_thermal;
    NSDate* _history_thermal_observe_time;

    BOOL _stop;
    int _temp;
    double _temp2;
}

@synthesize frameQueue = _frameQueue;
@synthesize captureSession = _captureSession;

- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate {
    return [self initWithDelegate:delegate captureSession:[[AVCaptureSession alloc] init]];
}

// This initializer is used for testing.
- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate
                  captureSession:(AVCaptureSession *)captureSession {
    if (self = [super initWithDelegate:delegate]) {
        // Create the capture session and all relevant inputs and outputs. We need
        // to do this in init because the application may want the capture session
        // before we start the capturer for e.g. AVCapturePreviewLayer. All objects
        // created here are retained until dealloc and never recreated.
        if (![self setupCaptureSession:captureSession]) {
            return nil;
        }
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if TARGET_OS_IPHONE
        _orientation = UIDeviceOrientationPortrait;
        _rotation = RTCVideoRotation_90;
        [center addObserver:self
                   selector:@selector(deviceOrientationDidChange:)
                       name:UIDeviceOrientationDidChangeNotification
                     object:nil];
        [center addObserver:self
                   selector:@selector(handleCaptureSessionInterruption:)
                       name:AVCaptureSessionWasInterruptedNotification
                     object:_captureSession];
        [center addObserver:self
                   selector:@selector(handleCaptureSessionInterruptionEnded:)
                       name:AVCaptureSessionInterruptionEndedNotification
                     object:_captureSession];
        [center addObserver:self
                   selector:@selector(handleApplicationDidBecomeActive:)
                       name:UIApplicationDidBecomeActiveNotification
                     object:[UIApplication sharedApplication]];
#endif
        [center addObserver:self
                   selector:@selector(handleCaptureSessionRuntimeError:)
                       name:AVCaptureSessionRuntimeErrorNotification
                     object:_captureSession];
        [center addObserver:self
                   selector:@selector(handleCaptureSessionDidStartRunning:)
                       name:AVCaptureSessionDidStartRunningNotification
                     object:_captureSession];
        [center addObserver:self
                   selector:@selector(handleCaptureSessionDidStopRunning:)
                       name:AVCaptureSessionDidStopRunningNotification
                     object:_captureSession];
    }
    return self;
}

- (void)dealloc {
    NSAssert(
             !_willBeRunning,
             @"Session was still running in RTCCameraVideoCapturer dealloc. Forgot to call stopCapture?");
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

+ (NSArray<AVCaptureDevice *> *)captureDevices {
    AVCaptureDeviceDiscoverySession *session = [AVCaptureDeviceDiscoverySession
                                                discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInTrueDepthCamera] mediaType:AVMediaTypeVideo
                                                position:AVCaptureDevicePositionUnspecified];
    return session.devices;
}

+ (NSArray<AVCaptureDeviceFormat *> *)supportedFormatsForDevice:(AVCaptureDevice *)device {
    // Support opening the device in any format. We make sure it's converted to a format we
    // can handle, if needed, in the method `-setupVideoDataOutput`.
    return device.formats;
}

- (FourCharCode)preferredOutputPixelFormat {
    return _preferredOutputPixelFormat;
}

- (void)startCaptureWithDevice:(AVCaptureDevice *)device
                        format:(AVCaptureDeviceFormat *)format
                           fps:(NSInteger)fps {
    [self startCaptureWithDevice:device format:format fps:fps completionHandler:nil];
}

- (void)stopCapture {
    [self stopCaptureWithCompletionHandler:nil];
}

- (void)startCaptureWithDevice:(AVCaptureDevice *)device
                        format:(AVCaptureDeviceFormat *)format
                           fps:(NSInteger)fps
             completionHandler:(nullable void (^)(NSError *))completionHandler {
    _willBeRunning = YES;
    _receive_first_depth = false;
    _buffer_queue_lock = dispatch_semaphore_create(kNumBuffer);
    _segmenter =[[SegmentApis alloc] initWithOutputWidth:640 OutputHeight:480];
    _index = 0;

    [RTCDispatcher
     dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
     block:^{
         RTCLogInfo("startCaptureWithDevice %@ @ %ld fps", format, (long)fps);

#if TARGET_OS_IPHONE
         [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
#endif

         _currentDevice = device;

         NSError *error = nil;
         if (![_currentDevice lockForConfiguration:&error]) {
             RTCLogError(
                         @"Failed to lock device %@. Error: %@", _currentDevice, error.userInfo);
             if (completionHandler) {
                 completionHandler(error);
             }
             _willBeRunning = NO;
             return;
         }
         [self reconfigureCaptureSessionInput];
         [self updateOrientation];
         [self updateDeviceCaptureFormat:format fps:fps];
         [self updateVideoDataOutputPixelFormat:format];

         AVCaptureDataOutputSynchronizer* syncOutput = [[AVCaptureDataOutputSynchronizer alloc] initWithDataOutputs:@[_videoDataOutput, _depthDataOutput]];
         [syncOutput setDelegate:self queue:self.frameQueue];
         _syncOutput = syncOutput;

         _history_thermal = NSProcessInfoThermalStateNominal;
         _history_thermal_observe_time = [NSDate date];

         [_captureSession startRunning];
         [_currentDevice unlockForConfiguration];
         _isRunning = YES;
         if (completionHandler) {
             completionHandler(nil);
         }
     }];

    _stop = false;
    //
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeProcess2
                                 block:^{
                                     while (true) {
                                       NSDate* begin = [NSDate date];
                                       // Add workload here.
                                         int temp=_temp2 / 255.0;
                                         for (int i = 0; i<100000; i++) {
                                             temp = (temp*5+21)%123457;
                                             if (temp%2==0){
                                                 if (temp > 1234567 / 2)
                                                   _temp2 = temp*255.0;
                                                 else
                                                   _temp2 = temp*255.0 - 2.0;
                                             } else {
                                                 if (temp % 2 == 0)
                                                   _temp2 += temp/253.0;
                                                 else
                                                     _temp2 -= temp/251.0;
                                             }
                                         }
                                         _temp = temp;
                                       NSTimeInterval execution = -[begin timeIntervalSinceNow];
                                       NSTimeInterval rest = execution * 1.5;
                                         //NSLog(@"QiangChen: Fake WorkLoad: %lf %lf", execution*1000, rest*1000);
                                       [NSThread sleepForTimeInterval: rest];
                                         if (_stop) break;
                                     }
                                 }];
    //*/
}

- (void)stopCaptureWithCompletionHandler:(nullable void (^)(void))completionHandler {
    _stop = true;
    _willBeRunning = NO;
    [RTCDispatcher
     dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
     block:^{
         RTCLogInfo("Stop");
         _currentDevice = nil;
         for (AVCaptureDeviceInput *oldInput in [_captureSession.inputs copy]) {
             [_captureSession removeInput:oldInput];
         }
         [_captureSession stopRunning];

#if TARGET_OS_IPHONE
         [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
#endif
         _isRunning = NO;
         if (completionHandler) {
             completionHandler();
         }
     }];
}

#pragma mark iOS notifications

#if TARGET_OS_IPHONE
- (void)deviceOrientationDidChange:(NSNotification *)notification {
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     [self updateOrientation];
                                 }];
}
#endif

#pragma mark AVCaptureVideoDataOutputSampleBufferDelegate
- (void)initializeBuffersWithColorBuffer:(CVPixelBufferRef) pixelBuffer
                             DepthBuffer:(CVPixelBufferRef) depth_image {
    int depthHeight = CVPixelBufferGetHeight(depth_image);
    int depthWidth = CVPixelBufferGetWidth(depth_image);
    OSType depthType = CVPixelBufferGetPixelFormatType(depth_image);
    CVPixelBufferCreate(nil, depthWidth, depthHeight,
                        depthType, nil, &_last_depth_buffer);
    NSLog(@"QiangChen: Depth Buffer OSType=0x%x Width=%d Height=%d", depthType, depthWidth, depthHeight);

    int colorHeight = CVPixelBufferGetHeight(pixelBuffer);
    int colorWidth = CVPixelBufferGetWidth(pixelBuffer);
    OSType colorType = CVPixelBufferGetPixelFormatType(pixelBuffer);
    CVPixelBufferCreate(nil, colorWidth, colorHeight,
                        colorType, nil, &_last_color_buffer);
    NSLog(@"QiangChen: Color Buffer OSType=0x%x Width=%d Height=%d", colorType, colorWidth, colorHeight);

    for (int i = 0; i < kNumBuffer; i++) {
        _buffer_queue_mark[i] = true;

        CVPixelBufferCreate(nil, colorWidth, colorHeight, kCVPixelFormatType_OneComponent8, nil, &_mask_buffer_queue[i]);
        CVPixelBufferCreate(nil, colorWidth, colorHeight, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, nil, &_out_color_buffer_queue[i]);


        CVPixelBufferCreate(nil, depthWidth, depthHeight, depthType, nil, &_depth_buffer_queue[i]);
        CVPixelBufferCreate(nil, colorWidth, colorHeight, colorType, nil, &_color_buffer_queue[i]);
        CVPixelBufferCreate(nil, colorWidth, colorHeight, colorType, nil, &_pre_color_buffer_queue[i]);
    }
}

- (int)getAndLockAnIndexWithTimeout: (dispatch_time_t) time_out {
    if (dispatch_semaphore_wait(_buffer_queue_lock, time_out) == 0){
        for (int i = 0; i<kNumBuffer; i++)
        {
            if (_buffer_queue_mark[i]) {
                // Mark the buffer as unavailable.
                _buffer_queue_mark[i] = false;
                return i;
            }
        }
        return -2;
    }
    else {
        return -1;
    }
}

- (void)returnAndUnlockAnIndex: (int) index {
    _buffer_queue_mark[index] = true;
    dispatch_semaphore_signal(_buffer_queue_lock);
}

- (void)copyDepthBufferFrom:(CVPixelBufferRef) source
                         To:(CVPixelBufferRef) destination {
        CVPixelBufferLockBaseAddress(source, kCVPixelBufferLock_ReadOnly);
        CVPixelBufferLockBaseAddress(destination, 0);

        float* destination_data = (float*)(CVPixelBufferGetBaseAddress(destination));
        const float* source_data = (const float*)(CVPixelBufferGetBaseAddress(source));

        int bufferHeight = CVPixelBufferGetHeight(source);
        int bytesPerRow = CVPixelBufferGetBytesPerRow(source);
        memcpy(destination_data, source_data, bufferHeight*bytesPerRow);

        CVPixelBufferUnlockBaseAddress(source, kCVPixelBufferLock_ReadOnly);
        CVPixelBufferUnlockBaseAddress(destination, 0);

}
- (void)copyColorBufferFrom:(CVPixelBufferRef) color_src
                         To:(CVPixelBufferRef) color_dst {
    CVPixelBufferLockBaseAddress(color_src, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferLockBaseAddress(color_dst, 0);

    uint8_t* destY = (uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_dst, 0));
    const uint8_t* srcY = (const uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_src, 0));

    int bufferHeight = CVPixelBufferGetHeightOfPlane(color_src, 0);
    int bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(color_src , 0);
    memcpy(destY, srcY, bufferHeight*bytesPerRow);

    uint8_t* destUV = (uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_dst, 1));
    const uint8_t* srcUV = (const uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_src, 1));

    bufferHeight = CVPixelBufferGetHeightOfPlane(color_src, 1);
    bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(color_src , 1);
    memcpy(destUV, srcUV, bufferHeight*bytesPerRow);

    CVPixelBufferUnlockBaseAddress(color_src, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferUnlockBaseAddress(color_dst, 0);
}

- (void)segmentAndStreamDownAtIndex: (int) index
                      withTimestamp: (int64_t) timestamp
                   withCurrentDepth: (bool) with_depth{
    //NSDate* begin = [NSDate date];
    if (with_depth){
        [_segmenter rgbdSegment:_color_buffer_queue[index]
                    depthBuffer:_depth_buffer_queue[index]
              outputColorBuffer:_out_color_buffer_queue[index]
                     maskBuffer:_mask_buffer_queue[index]
                      fillColor:false
                          index:_index
                   snapshotPath:@""];
    }
    else{
        [_segmenter rgbdSegment:_color_buffer_queue[index]
                 preColorBuffer:_pre_color_buffer_queue[index]
                 preDepthBuffer:_depth_buffer_queue[index]
              outputColorBuffer:_out_color_buffer_queue[index]
                     maskBuffer:_mask_buffer_queue[index]
                      fillColor:false
                          index:_index
                   snapshotPath:@""];
    }
    //NSTimeInterval interval = -[begin timeIntervalSinceNow];
    //NSLog(@"QiangChen: Segment Timespan: %lfms", interval*1000);
    RTCCVPixelBuffer *rtcPixelBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_out_color_buffer_queue[index]];
    RTCCVPixelBuffer *rtcMaskBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_mask_buffer_queue[index]];

    RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                      withDepthBuffer:rtcMaskBuffer
                                                             rotation:_rotation
                                                          timeStampNs:timestamp];

    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypePostProcess
                                 block:^{
                                     [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
                                     [self returnAndUnlockAnIndex:index];
                                 }];
}

- (void)dataOutputSynchronizer:(AVCaptureDataOutputSynchronizer *)synchronizer
didOutputSynchronizedDataCollection:(AVCaptureSynchronizedDataCollection *)synchronizedDataCollection {
    AVCaptureSynchronizedSampleBufferData* videoData =
    (AVCaptureSynchronizedSampleBufferData*)[synchronizedDataCollection synchronizedDataForCaptureOutput:_videoDataOutput];
    _index++;
    CMSampleBufferRef sampleBuffer = [videoData sampleBuffer];

    if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 || !CMSampleBufferIsValid(sampleBuffer) ||
        !CMSampleBufferDataIsReady(sampleBuffer)) {
        return;
    }

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (pixelBuffer == nil) {
        return;
    }

    AVCaptureSynchronizedDepthData* depthData =
    (AVCaptureSynchronizedDepthData*)[synchronizedDataCollection synchronizedDataForCaptureOutput:_depthDataOutput];

    if (depthData!=nil) {
        _receive_first_depth = true;
        CVPixelBufferRef depth_image = depthData.depthData.depthDataMap;
        if (_last_depth_buffer==nil) {
            [self initializeBuffersWithColorBuffer:pixelBuffer DepthBuffer:depth_image];
        }
        [self copyDepthBufferFrom:depth_image To:_last_depth_buffer];
    }

    if (_receive_first_depth == false) {
        return;
    }

    int64_t timeStampNs = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) *
    kNanosecondsPerSecond;

    NSProcessInfoThermalState thermal = [[NSProcessInfo processInfo] thermalState];

    if (thermal != _history_thermal) {
        NSTimeInterval timeInterval = -[_history_thermal_observe_time timeIntervalSinceNow];
        _history_thermal = thermal;
        _history_thermal_observe_time = [NSDate date];
        RTCLogError("Qiang Chen: Timespan: %lf seconds Thermal: %s", timeInterval,
                    thermal == NSProcessInfoThermalStateFair ? "Fair" :
                    thermal == NSProcessInfoThermalStateNominal ? "Nominal" :
                    thermal == NSProcessInfoThermalStateSerious ? "Serious" :
                    thermal == NSProcessInfoThermalStateCritical ? "Critical" :"Unknown");
    }

    // Debug: No Segmentation
    RTCCVPixelBuffer *rtcPixelBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer];
    RTCCVPixelBuffer *rtcMaskBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_last_depth_buffer];

    RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                      withDepthBuffer:rtcMaskBuffer
                                                             rotation:_rotation
                                                          timeStampNs:timeStampNs];
    [self.delegate capturer:self didCaptureVideoFrame:videoFrame];

    return;
    // Debug: No Segmentation*/

    int buffer_index = [self getAndLockAnIndexWithTimeout:DISPATCH_TIME_NOW];
    // No Buffer Available, drop the frame;
    if (buffer_index < 0) {
        RTCLogError("Qiang Chen: Drop Frame Due To Buffer Unavailable!");
        if (buffer_index == -2)
            RTCLogError("Qiang Chen: Drop Frame: Severe Error Buffer Management Algorithm Is Problematic!");
        return;
    }

    [self copyColorBufferFrom:pixelBuffer To:_color_buffer_queue[buffer_index]];
    if (depthData == nil)
      [self copyColorBufferFrom:_last_color_buffer To:_pre_color_buffer_queue[buffer_index]];
    [self copyDepthBufferFrom:_last_depth_buffer To:_depth_buffer_queue[buffer_index]];
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeProcess
                                 block:^{
                                     [self segmentAndStreamDownAtIndex:buffer_index
                                                         withTimestamp:timeStampNs
                                                      withCurrentDepth:(depthData != nil)];
                                 }];

    [self copyColorBufferFrom:pixelBuffer To:_last_color_buffer];

#if TARGET_OS_IPHONE
    // Default to portrait orientation on iPhone.
    BOOL usingFrontCamera = YES;
    switch (_orientation) {
        case UIDeviceOrientationPortrait:
            _rotation = RTCVideoRotation_90;
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            _rotation = RTCVideoRotation_270;
            break;
        case UIDeviceOrientationLandscapeLeft:
            _rotation = usingFrontCamera ? RTCVideoRotation_180 : RTCVideoRotation_0;
            break;
        case UIDeviceOrientationLandscapeRight:
            _rotation = usingFrontCamera ? RTCVideoRotation_0 : RTCVideoRotation_180;
            break;
        case UIDeviceOrientationFaceUp:
        case UIDeviceOrientationFaceDown:
        case UIDeviceOrientationUnknown:
            // Ignore.
            break;
    }
#else
    // No rotation on Mac.
    _rotation = RTCVideoRotation_0;
#endif
}
- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    NSLog(@"QiangChen: Orphan Color Output %lf", CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)));
}
- (void)captureOutput:(AVCaptureOutput *)output
  didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection{

}
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)output
     didOutputDepthData:(AVDepthData *)depthData
              timestamp:(CMTime)timestamp
             connection:(AVCaptureConnection *)connection{
    NSLog(@"QiangChen: Orphan Depth Output %lf", CMTimeGetSeconds(timestamp));

}
- (void)depthDataOutput:(AVCaptureDepthDataOutput *)output
       didDropDepthData:(AVDepthData *)depthData
              timestamp:(CMTime)timestamp
             connection:(AVCaptureConnection *)connection
                 reason:(AVCaptureOutputDataDroppedReason)reason{
    
}
#pragma mark - AVCaptureSession notifications

- (void)handleCaptureSessionInterruption:(NSNotification *)notification {
    NSString *reasonString = nil;
#if TARGET_OS_IPHONE
    NSNumber *reason = notification.userInfo[AVCaptureSessionInterruptionReasonKey];
    if (reason) {
        switch (reason.intValue) {
            case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground:
                reasonString = @"VideoDeviceNotAvailableInBackground";
                break;
            case AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient:
                reasonString = @"AudioDeviceInUseByAnotherClient";
                break;
            case AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient:
                reasonString = @"VideoDeviceInUseByAnotherClient";
                break;
            case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps:
                reasonString = @"VideoDeviceNotAvailableWithMultipleForegroundApps";
                break;
        }
    }
#endif
    RTCLog(@"Capture session interrupted: %@", reasonString);
}

- (void)handleCaptureSessionInterruptionEnded:(NSNotification *)notification {
    RTCLog(@"Capture session interruption ended.");
}

- (void)handleCaptureSessionRuntimeError:(NSNotification *)notification {
    NSError *error = [notification.userInfo objectForKey:AVCaptureSessionErrorKey];
    RTCLogError(@"Capture session runtime error: %@", error);

    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
#if TARGET_OS_IPHONE
                                     if (error.code == AVErrorMediaServicesWereReset) {
                                         [self handleNonFatalError];
                                     } else {
                                         [self handleFatalError];
                                     }
#else
                                     [self handleFatalError];
#endif
                                 }];
}

- (void)handleCaptureSessionDidStartRunning:(NSNotification *)notification {
    RTCLog(@"Capture session started.");

    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     // If we successfully restarted after an unknown error,
                                     // allow future retries on fatal errors.
                                     _hasRetriedOnFatalError = NO;
                                 }];
}

- (void)handleCaptureSessionDidStopRunning:(NSNotification *)notification {
    RTCLog(@"Capture session stopped.");
}

- (void)handleFatalError {
    [RTCDispatcher
     dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
     block:^{
         if (!_hasRetriedOnFatalError) {
             RTCLogWarning(@"Attempting to recover from fatal capture error.");
             [self handleNonFatalError];
             _hasRetriedOnFatalError = YES;
         } else {
             RTCLogError(@"Previous fatal error recovery failed.");
         }
     }];
}

- (void)handleNonFatalError {
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     RTCLog(@"Restarting capture session after error.");
                                     if (_isRunning) {
                                         [_captureSession startRunning];
                                     }
                                 }];
}

#if TARGET_OS_IPHONE

#pragma mark - UIApplication notifications

- (void)handleApplicationDidBecomeActive:(NSNotification *)notification {
    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                 block:^{
                                     if (_isRunning && !_captureSession.isRunning) {
                                         RTCLog(@"Restarting capture session on active.");
                                         [_captureSession startRunning];
                                     }
                                 }];
}

#endif  // TARGET_OS_IPHONE

#pragma mark - Private

- (dispatch_queue_t)frameQueue {
    if (!_frameQueue) {
        _frameQueue =
        dispatch_queue_create("org.webrtc.cameravideocapturer.video", DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(_frameQueue,
                                  dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    }
    return _frameQueue;
}

- (BOOL)setupCaptureSession:(AVCaptureSession *)captureSession {
    NSAssert(_captureSession == nil, @"Setup capture session called twice.");
    _captureSession = captureSession;
#if defined(WEBRTC_IOS)
    _captureSession.sessionPreset = AVCaptureSessionPresetInputPriority;
    _captureSession.usesApplicationAudioSession = NO;
#endif
    [self setupVideoDataOutput];
    // Add the output.
    if (![_captureSession canAddOutput:_videoDataOutput]) {
        RTCLogError(@"Video data output unsupported.");
        return NO;
    }
    [_captureSession addOutput:_videoDataOutput];

    if (![_captureSession canAddOutput:_depthDataOutput]) {
        RTCLogError(@"QiangChen: Depth data output unsupported.");
        return NO;
    } else {
        [_captureSession addOutput:_depthDataOutput];
    }
    
    return YES;
}

- (void)setupVideoDataOutput {
    NSAssert(_videoDataOutput == nil, @"Setup video data output called twice.");
    AVCaptureVideoDataOutput *videoDataOutput = [[AVCaptureVideoDataOutput alloc] init];

    // `videoDataOutput.availableVideoCVPixelFormatTypes` returns the pixel formats supported by the
    // device with the most efficient output format first. Find the first format that we support.
    NSSet<NSNumber *> *supportedPixelFormats = [RTCCVPixelBuffer supportedPixelFormats];
    NSMutableOrderedSet *availablePixelFormats =
    [NSMutableOrderedSet orderedSetWithArray:videoDataOutput.availableVideoCVPixelFormatTypes];
    [availablePixelFormats intersectSet:supportedPixelFormats];
    NSNumber *pixelFormat = availablePixelFormats.firstObject;
    NSAssert(pixelFormat, @"Output device has no supported formats.");

    _preferredOutputPixelFormat = [pixelFormat unsignedIntValue];
    _outputPixelFormat = _preferredOutputPixelFormat;
    videoDataOutput.videoSettings = @{(NSString *)kCVPixelBufferPixelFormatTypeKey : pixelFormat};
    videoDataOutput.alwaysDiscardsLateVideoFrames = NO;
    [videoDataOutput setSampleBufferDelegate:self queue:self.frameQueue];
    _videoDataOutput = videoDataOutput;

    AVCaptureDepthDataOutput* depthDataOutput = [[AVCaptureDepthDataOutput alloc] init];
    depthDataOutput.alwaysDiscardsLateDepthData = NO;
    depthDataOutput.filteringEnabled = NO;
    [depthDataOutput setDelegate:self callbackQueue:self.frameQueue];
    _depthDataOutput = depthDataOutput;
}

- (void)updateVideoDataOutputPixelFormat:(AVCaptureDeviceFormat *)format {
    FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    if (![[RTCCVPixelBuffer supportedPixelFormats] containsObject:@(mediaSubType)]) {
        mediaSubType = _preferredOutputPixelFormat;
    }

    if (mediaSubType != _outputPixelFormat) {
        _outputPixelFormat = mediaSubType;
        _videoDataOutput.videoSettings =
        @{ (NSString *)kCVPixelBufferPixelFormatTypeKey : @(mediaSubType) };
    }
}

#pragma mark - Private, called inside capture queue

- (void)updateDeviceCaptureFormat:(AVCaptureDeviceFormat *)format fps:(NSInteger)fps {
    NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
             @"updateDeviceCaptureFormat must be called on the capture queue.");
    @try {
        if (fps>30) fps=30;

        _currentDevice.activeFormat = format;
        _currentDevice.activeVideoMinFrameDuration = CMTimeMake(1, fps);

        CMFormatDescriptionRef colorFormatDesc = [format formatDescription];
        CMVideoDimensions colorDimension = CMVideoFormatDescriptionGetDimensions(colorFormatDesc);

        NSArray<AVCaptureDeviceFormat *> * depthFormats = [format supportedDepthDataFormats];
        NSLog(@"QiangChen: DepthFormats %d",(int)[depthFormats count]);
        AVCaptureDeviceFormat* selectedDepthFormat = depthFormats[0];
        for (AVCaptureDeviceFormat* depthFormat in depthFormats) {
            CMFormatDescriptionRef formatDesc = [depthFormat formatDescription];
            CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(formatDesc);
            OSType mtype = CMFormatDescriptionGetMediaSubType(formatDesc);
            if (mtype != kCVPixelFormatType_DepthFloat32) continue;
            selectedDepthFormat = depthFormat;
            if (colorDimension.width == dimension.width*2 && colorDimension.height == dimension.height*2)
            {
                selectedDepthFormat = depthFormat;
                break;
            }
        }
        _currentDevice.activeDepthDataFormat = selectedDepthFormat;
    } @catch (NSException *exception) {
        RTCLogError(@"Failed to set active format!\n User info:%@", exception.userInfo);
        return;
    }
}

- (void)reconfigureCaptureSessionInput {
    NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
             @"reconfigureCaptureSessionInput must be called on the capture queue.");
    NSError *error = nil;
    AVCaptureDeviceInput *input =
    [AVCaptureDeviceInput deviceInputWithDevice:_currentDevice error:&error];
    if (!input) {
        RTCLogError(@"Failed to create front camera input: %@", error.localizedDescription);
        return;
    }
    [_captureSession beginConfiguration];
    for (AVCaptureDeviceInput *oldInput in [_captureSession.inputs copy]) {
        [_captureSession removeInput:oldInput];
    }
    if ([_captureSession canAddInput:input]) {
        [_captureSession addInput:input];
    } else {
        RTCLogError(@"Cannot add camera as an input to the session.");
    }
    [_captureSession commitConfiguration];
}

- (void)updateOrientation {
    NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
             @"updateOrientation must be called on the capture queue.");
#if TARGET_OS_IPHONE
    _orientation = [UIDevice currentDevice].orientation;
#endif
}

@end
