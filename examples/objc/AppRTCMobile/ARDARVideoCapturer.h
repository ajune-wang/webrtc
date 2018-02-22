//
//  ARDARVideoCapturer.h
//  sources
//
//  Created by Anders Carlsson on 2017-12-08.
//

#import <ARKit/ARKit.h>
#import <Foundation/Foundation.h>

#import <WebRTC/RTCVideoCapturer.h>

@interface ARDARVideoCapturer : RTCVideoCapturer <ARSessionDelegate>

- (void)startCapture;
- (void)stopCapture;

@end
