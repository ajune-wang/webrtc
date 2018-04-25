/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLRGBRenderer.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#import "RTCMTLRenderer+Private.h"
#include "rtc_base/checks.h"

#define MTL_STRINGIFY(s) @ #s

static NSString *const shaderSource = MTL_STRINGIFY(
    using namespace metal; typedef struct {
      packed_float2 position;
      packed_float2 texcoord;
    } Vertex;

    typedef struct {
      float4 position[[position]];
      float2 texcoord;
    } Varyings;

    vertex Varyings vertexPassthrough(device Vertex * verticies[[buffer(0)]],
                                      unsigned int vid[[vertex_id]]) {
      Varyings out;
      device Vertex &v = verticies[vid];
      out.position = float4(float2(v.position), 0.0, 1.0);
      out.texcoord = v.texcoord;
      return out;
    }

    fragment half4 fragmentColorConversion(
        Varyings in[[stage_in]], texture2d<float, access::sample> texture[[texture(0)]],
                                           constant bool* isRGBA[[buffer(0)]]) {
      constexpr sampler s(address::clamp_to_edge, filter::linear);

      float4 out = texture.sample(s, in.texcoord);
      if (isRGBA[0] != 0) {
        out = float4(out.g, out.b, out.a, out.r);
      }

      return half4(out);
    });

@implementation RTCMTLRGBRenderer {
  // Textures.
  CVMetalTextureCacheRef _textureCache;
  id<MTLTexture> _texture;

  // Uniforms.
  bool _isRGBA;  //anything !=0 means RGBA, 0 means ARGB
  id<MTLBuffer> _uniformsBuffer;
}

- (BOOL)addRenderingDestination:(__kindof MTKView *)view {
  if ([super addRenderingDestination:view]) {
    [self initializeTextureCache];
    return YES;
  }
  return NO;
}

- (void)initializeTextureCache {
  CVReturn status = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, [self currentMetalDevice],
                                              nil, &_textureCache);
  if (status != kCVReturnSuccess) {
    RTCLogError(@"Metal: Failed to initialize metal texture cache. Return status is %d", status);
  }
}

- (NSString *)shaderSource {
  return shaderSource;
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  RTC_DCHECK([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]);
  [super setupTexturesForFrame:frame];
  CVPixelBufferRef pixelBuffer = ((RTCCVPixelBuffer *)frame.buffer).pixelBuffer;

  id<MTLTexture> texture = nil;
  CVMetalTextureRef outTexture = nullptr;

  int width = CVPixelBufferGetWidth(pixelBuffer);
  int height = CVPixelBufferGetHeight(pixelBuffer);
  OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

  MTLPixelFormat mtlPixelFormat;
  if (pixelFormat == kCVPixelFormatType_32BGRA) {
    mtlPixelFormat = MTLPixelFormatBGRA8Unorm;
    _isRGBA = 0;
  } else if (pixelFormat == kCVPixelFormatType_32ARGB) {
    mtlPixelFormat = MTLPixelFormatRGBA8Unorm;
    _isRGBA = 1;
  } else {
    return NO;
  }

  CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
                kCFAllocatorDefault, _textureCache, pixelBuffer, nil, mtlPixelFormat,
                width, height, 0, &outTexture);
  if (result == kCVReturnSuccess) {
    texture =  CVMetalTextureGetTexture(outTexture);
  }
  CVBufferRelease(outTexture);

  if (texture != nil) {
    _texture = texture;
    _uniformsBuffer =
        [[self currentMetalDevice] newBufferWithBytes:&_isRGBA
                                               length:sizeof(_isRGBA)
                                              options:MTLResourceCPUCacheModeDefaultCache];
    return YES;
  }

  return NO;
}

- (void)uploadTexturesToRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder {
  [renderEncoder setFragmentTexture:_texture atIndex:0];
  [renderEncoder setFragmentBuffer:_uniformsBuffer offset:0 atIndex:0];
}

- (void)dealloc {
  if (_textureCache) {
    CFRelease(_textureCache);
  }
}

@end
