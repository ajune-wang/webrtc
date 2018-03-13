//
//  SegmentApis.h
//  SegmentAlgorithms
//
//  Created by George Zhou on 2/7/18.
//  Copyright © 2018 George Zhou. All rights reserved.
//

// This class provides APIs for RGBD Segmentation.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface SegmentApis : NSObject

// RGBD segmentation based on UIImages.
//
// @param color: the color RGB image.
// @param depthImage: the depth image.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
// @returns the segmented color RGB image.
- (UIImage *)rgbdSegmentUIImage:(UIImage *)color depthImage:(UIImage*)depth index:(int)index snapshotPath:(NSString*)path;

// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image.
// @param depth: the depth image.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
// @returns the segmented color RGB image.
- (UIImage *)rgbdSegmentPixelBuffer:(CVPixelBufferRef)color depthBuffer:(CVPixelBufferRef)depth index:(int)index snapshotPath:(NSString*)path;

// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image. It will be filled with the segmented RGB image as output.
// @param depth: the depth image.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
- (void)rgbdSegmentPixelBuffers:(CVPixelBufferRef)color depthBuffer:(CVPixelBufferRef)depth index:(int)index snapshotPath:(NSString*)path;
// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image. It will be filled with the segmented RGB image as output.
// @param depth: the depth image.
// @param mask:  The output foreground segmentation mask.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
- (void)rgbdSegmentAlpha:(CVPixelBufferRef)color depthBuffer:(CVPixelBufferRef)depth maskBuffer:(CVPixelBufferRef)mask index:(int)index snapshotPath:(NSString*)path;

// Set a background image.
- (void)setBackground:(UIImage*)background;

@end
