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

#define kNumBuffer 4

const int64_t kNanosecondsPerSecond = 1000000000;

@implementation ARDARVideoCapturer {
    ARSession *_arSession;
    RTCVideoRotation _rotation;
#if TARGET_OS_IPHONE
    UIDeviceOrientation _orientation;
    SegmentApis* _segmenter;
    int _index;
    bool _receive_first_depth;

    // Debug Purpose
    int _depth_num;
    NSDate* _depth_ref;

    int _all_num;
    NSDate* _all_ref;

    // FPS Control
    int _effect_count;
    NSDate* _fps_ref;

    // Buffer queue
    CVPixelBufferRef _last_depth_buffer; // Only accessible on Main thread
    CVPixelBufferRef _mask_buffer_queue[kNumBuffer];
    CVPixelBufferRef _depth_buffer_queue[kNumBuffer];
    CVPixelBufferRef _color_buffer_queue[kNumBuffer];
    bool _buffer_queue_mark[kNumBuffer]; // True means the buffer is free
    dispatch_semaphore_t _buffer_queue_lock;
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
                                     _depth_num = 0;
                                     _all_num = 0;
                                     _effect_count = 0;
                                     _receive_first_depth = false;
                                     CVPixelBufferCreate(nil, 640, 360,
                                                         kCVPixelFormatType_DepthFloat32, nil, &_last_depth_buffer);


                                     _buffer_queue_lock = dispatch_semaphore_create(kNumBuffer);
                                     for (int i = 0; i < kNumBuffer; i++) {
                                         _buffer_queue_mark[i] = true;

                                         CVPixelBufferCreate(nil, 640, 480, kCVPixelFormatType_OneComponent8, nil, &_mask_buffer_queue[i]);


                                         CVPixelBufferCreate(nil, 640, 360, kCVPixelFormatType_DepthFloat32, nil, &_depth_buffer_queue[i]);


                                         CVPixelBufferCreate(nil, 1280, 720, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, nil, &_color_buffer_queue[i]);
                                     }
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

                                     if (_last_depth_buffer != NULL) {
                                         CVPixelBufferRelease(_last_depth_buffer);
                                         _last_depth_buffer = NULL;
                                     }

                                     for (int i = 0; i < kNumBuffer; i++) {
                                         // Wait the pending tasks finishing and release the buffers.
                                         int index = [self getAndLockAnIndexWithTimeout:DISPATCH_TIME_FOREVER];

                                         if (_mask_buffer_queue[index] != NULL) {
                                             CVPixelBufferRelease(_mask_buffer_queue[index]);
                                             _mask_buffer_queue[index] = NULL;
                                         }
                                         if (_depth_buffer_queue[index] != NULL) {
                                             CVPixelBufferRelease(_depth_buffer_queue[index]);
                                             _depth_buffer_queue[index] = NULL;
                                         }
                                         if (_color_buffer_queue[index] != NULL) {
                                             CVPixelBufferRelease(_color_buffer_queue[index]);
                                             _color_buffer_queue[index] = NULL;
                                         }
                                     }

                                     for (int i = 0; i<kNumBuffer;i++){
                                         [self returnAndUnlockAnIndex: i];
                                     }
#endif
                                 }];
}

- (void)segmentAndStreamDownAtIndex: (int) index
                      withTimestamp: (int64_t) timestamp{
    NSDate* start = [NSDate date];
    RTCCVPixelBuffer *rtcPixelBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_color_buffer_queue[index]];


    [_segmenter rgbdSegment:_color_buffer_queue[index]
                     depthBuffer:_depth_buffer_queue[index]
                      maskBuffer:_mask_buffer_queue[index]
                       fillColor:false
                           index:_index
                    snapshotPath:@""];

    RTCCVPixelBuffer *rtcMaskBuffer =
    [[RTCCVPixelBuffer alloc] initWithPixelBuffer:_mask_buffer_queue[index]];

    RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                      withDepthBuffer:rtcMaskBuffer
                                                             rotation:_rotation
                                                          timeStampNs:timestamp];
    NSTimeInterval timeInterval = -[start timeIntervalSinceNow];
    NSProcessInfoThermalState thermal = [[NSProcessInfo processInfo] thermalState];

    RTCLogError("Qiang Chen: Segment Timespan: %lf Thermal: %s", timeInterval * 1000,
                thermal == NSProcessInfoThermalStateFair ? "Fair" :
                thermal == NSProcessInfoThermalStateNominal ? "Nominal" :
                thermal == NSProcessInfoThermalStateSerious ? "Serious" :
                thermal == NSProcessInfoThermalStateCritical ? "Critical" :"Unknown");

    [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypePostProcess
                             block:^{
                                NSDate* start = [NSDate date];
                                [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
                                NSTimeInterval timeInterval = -[start timeIntervalSinceNow];

                                RTCLogError("Qiang Chen: PostSegment Timespan: %lf", timeInterval * 1000);

                                [self returnAndUnlockAnIndex:index];
                             }];
}

#pragma mark ARSessionDelegate
- (void)session:(ARSession *)sessionn didUpdateFrame:(ARFrame *)frame {
    NSAssert(frame != nil, @"ARFrame is empty.");
    AVDepthData* depth_data = [frame capturedDepthData];

    if (!depth_data && _receive_first_depth==false) return;
    _receive_first_depth = true;
    _index++;

    //DEBUG
    if (_all_num % 100 == 0) {
        _all_num = 0;
        RTCLogError("Qiang Chen: Incoming Color FPS: %lf", -100/[_all_ref timeIntervalSinceNow]);
        _all_ref = [NSDate date];
    }
    _all_num++;
    //DEBUG

    if (depth_data){
        //DEBUG
        if (_depth_num%100 == 0)
        {
            _depth_num = 0;
            RTCLogError("Qiang Chen: Incoming Depth FPS: %lf", -100/[_depth_ref timeIntervalSinceNow]);
            _depth_ref = [NSDate date];
        }
        _depth_num++;
        //DEBUG

       CVPixelBufferRef depth_buffer = [depth_data depthDataMap];
       if (_last_depth_buffer != NULL) {
            CVPixelBufferLockBaseAddress(depth_buffer, kCVPixelBufferLock_ReadOnly);
            CVPixelBufferLockBaseAddress(_last_depth_buffer, 0);

            float* _last_depth_buffer_data = (float*)(CVPixelBufferGetBaseAddress(_last_depth_buffer));
            const float* depth_buffer_data = (const float*)(CVPixelBufferGetBaseAddress(depth_buffer));

            int bufferHeight = CVPixelBufferGetHeight(depth_buffer);
            int bytesPerRow = CVPixelBufferGetBytesPerRow(depth_buffer);
            memcpy(_last_depth_buffer_data, depth_buffer_data, bufferHeight*bytesPerRow);

            CVPixelBufferUnlockBaseAddress(depth_buffer, kCVPixelBufferLock_ReadOnly);
            CVPixelBufferUnlockBaseAddress(_last_depth_buffer, 0);
        }
    }

    if (_effect_count%100 == 0) {
        _effect_count = 0;
        _fps_ref = [NSDate date];
    }

    if (_effect_count > 32 * (-[_fps_ref timeIntervalSinceNow])){
        RTCLogError("Drop Frame Due to FPS control");
        return;
    }

    int buffer_index = [self getAndLockAnIndexWithTimeout:DISPATCH_TIME_NOW];
    // No Buffer Available, drop the frame;
    if (buffer_index < 0) {
        RTCLogError("Qiang Chen: Drop Frame Due To Buffer Unavailable!");
        if (buffer_index == -2)
            RTCLogError("Qiang Chen: Drop Frame: Severe Error Buffer Management Algorithm Is Problematic!");
        return;
    } else {
        _effect_count ++;
    }

    // Copy Depth Data Into The Buffer
    CVPixelBufferLockBaseAddress(_last_depth_buffer, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferLockBaseAddress(_depth_buffer_queue[buffer_index], 0);

    float* depth_buffer_backup_data = (float*)(CVPixelBufferGetBaseAddress(_depth_buffer_queue[buffer_index]));
    const float* last_depth_buffer_data = (const float*)(CVPixelBufferGetBaseAddress(_last_depth_buffer));

    int bufferHeight = CVPixelBufferGetHeight(_last_depth_buffer);
    int bytesPerRow = CVPixelBufferGetBytesPerRow(_last_depth_buffer);
    memcpy(depth_buffer_backup_data, last_depth_buffer_data, bufferHeight*bytesPerRow);

    CVPixelBufferUnlockBaseAddress(_last_depth_buffer, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferUnlockBaseAddress(_depth_buffer_queue[buffer_index], 0);

    // Copy Color Data Into The Buffer
    CVPixelBufferRef color_frame = [frame capturedImage];
    CVPixelBufferLockBaseAddress(color_frame, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferLockBaseAddress(_color_buffer_queue[buffer_index], 0);

    uint8_t* destY = (uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(_color_buffer_queue[buffer_index], 0));
    const uint8_t* srcY = (const uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_frame, 0));

    bufferHeight = CVPixelBufferGetHeightOfPlane(color_frame, 0);
    bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(color_frame , 0);
    memcpy(destY, srcY, bufferHeight*bytesPerRow);

    uint8_t* destUV = (uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(_color_buffer_queue[buffer_index], 1));
    const uint8_t* srcUV = (const uint8_t*)(CVPixelBufferGetBaseAddressOfPlane(color_frame, 1));

    bufferHeight = CVPixelBufferGetHeightOfPlane(color_frame, 1);
    bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(color_frame , 1);
    memcpy(destUV, srcUV, bufferHeight*bytesPerRow);

    CVPixelBufferUnlockBaseAddress(color_frame, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferUnlockBaseAddress(_color_buffer_queue[buffer_index], 0);

    RTCDispatcherQueueType thread_const = (true)?RTCDispatcherTypeProcess:RTCDispatcherTypeProcess2;
    [RTCDispatcher dispatchAsyncOnType:thread_const
                                 block:^{
                                     [self segmentAndStreamDownAtIndex:buffer_index
                                                         withTimestamp:[frame timestamp] * kNanosecondsPerSecond];
                                 }];
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
    
    _segmenter =[[SegmentApis alloc] initWithOutputWidth:640 OutputHeight:480];
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
