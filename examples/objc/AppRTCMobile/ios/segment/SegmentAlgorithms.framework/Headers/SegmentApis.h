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
- (id)initWithOutputWidth:(int)width
             OutputHeight:(int)height;

// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image.
// @param depth: the depth image.
// @param mask:  The output foreground segmentation mask.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
- (void)rgbdSegment:(CVPixelBufferRef)color depthBuffer:(CVPixelBufferRef)depth
         maskBuffer:(CVPixelBufferRef)mask fillColor:(bool)fill index:(int)index
        snapshotPath:(NSString*)path;

// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image.
// @param preColor: The color RGB image of a previous frame.
// @param preDepth: the depth image of a previous frame.
// @param mask:  The output foreground segmentation mask.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
- (void)rgbdSegment:(CVPixelBufferRef)color preColorBuffer:(CVPixelBufferRef)preColor
                preDepthBuffer:(CVPixelBufferRef)preDepth maskBuffer:(CVPixelBufferRef)mask fillColor:(bool)fill
                index:(int)index snapshotPath:(NSString*)path;

// RGBD segmentation based on CVPixelBufferRefs.
//
// @param color: the color RGB image.
// @param preColor: The color RGB image of a previous frame.
// @param preMaskBuffer: the segmentation mask of a previous frame.
// @param mask:  The output foreground segmentation mask.
// @param index: the frame index.
// @param snapshotPath: the path for the snapshot.
- (void)rgbdSegment:(CVPixelBufferRef)color preColorBuffer:(CVPixelBufferRef)preColor
             preMaskBuffer:(CVPixelBufferRef)preMask maskBuffer:(CVPixelBufferRef)mask fillColor:(bool)fill
             index:(int)index snapshotPath:(NSString*)path;

// Set a background image.
- (void)setBackground:(UIImage*)background;

@end
