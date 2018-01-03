/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <memory>

#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))

#define JNI_FUNCTION_DECLARATION(rettype, name, ...)                   \
  extern "C" JNIEXPORT rettype JNICALL Java_org_appspot_apprtc_##name( \
      __VA_ARGS__)

#include <android/log.h>
#define ALOGE(...) \
  __android_log_print(ANDROID_LOG_ERROR, "AppRTCMobile", __VA_ARGS__)

#include "examples/androidapp/src/jni/include/fabby_sdk_videosegmentation.h"

#define GL_HALF_FLOAT 0x140B

class Float16Compressor {
  union Bits {
    float f;
    int32_t si;
    uint32_t ui;
  };

  static int const shift = 13;
  static int const shiftSign = 16;

  static int32_t const infN = 0x7F800000;   // flt32 infinity
  static int32_t const maxN = 0x477FE000;   // max flt16 normal as a flt32
  static int32_t const minN = 0x38800000;   // min flt16 normal as a flt32
  static int32_t const signN = 0x80000000;  // flt32 sign bit

  static int32_t const infC = infN >> shift;
  static int32_t const nanN = (infC + 1)
                              << shift;  // minimum flt16 nan as a flt32
  static int32_t const maxC = maxN >> shift;
  static int32_t const minC = minN >> shift;
  static int32_t const signC = signN >> shiftSign;  // flt16 sign bit

  static int32_t const mulN = 0x52000000;  // (1 << 23) / minN
  static int32_t const mulC = 0x33800000;  // minN / (1 << (23 - shift))

  static int32_t const subC = 0x003FF;  // max flt32 subnormal down shifted
  static int32_t const norC = 0x00400;  // min flt32 normal down shifted

  static int32_t const maxD = infC - maxC - 1;
  static int32_t const minD = minC - subC - 1;

 public:
  static uint16_t compress(float value) {
    Bits v, s;
    v.f = value;
    uint32_t sign = v.si & signN;
    v.si ^= sign;
    sign >>= shiftSign;  // logical shift
    s.si = mulN;
    s.si = s.f * v.f;  // correct subnormals
    v.si ^= (s.si ^ v.si) & -(minN > v.si);
    v.si ^= (infN ^ v.si) & -((infN > v.si) & (v.si > maxN));
    v.si ^= (nanN ^ v.si) & -((nanN > v.si) & (v.si > infN));
    v.ui >>= shift;  // logical shift
    v.si ^= ((v.si - maxD) ^ v.si) & -(v.si > maxC);
    v.si ^= ((v.si - minD) ^ v.si) & -(v.si > subC);
    return v.ui | sign;
  }

  static float decompress(uint16_t value) {
    Bits v;
    v.ui = value;
    int32_t sign = v.si & signC;
    v.si ^= sign;
    sign <<= shiftSign;
    v.si ^= ((v.si + minD) ^ v.si) & -(v.si > subC);
    v.si ^= ((v.si + maxD) ^ v.si) & -(v.si > maxC);
    Bits s;
    s.si = mulC;
    s.f *= v.si;
    int32_t mask = -(norC > v.si);
    v.si <<= shift;
    v.si ^= (s.si ^ v.si) & mask;
    v.si |= sign;
    return v.f;
  }
};

namespace AppRTCMobile {
namespace jni {

static const GLfloat g_vertex_buffer_data[] = {
    -1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
};

const GLchar* vert_src =
    "#version 100\n"
    "attribute vec3 Pos;\n"
    "void main() {\n"
    "   gl_Position = vec4(Pos.x, Pos.y, Pos.z, 1.0); }\n";

const GLchar* frag_src =
    "#version 100\n"
    "precision mediump float;                   \n"
    "void main() {\n"
    "   gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }\n";

GLuint shader_prog_id;
GLuint vertexBuffer;

void glCheckError(const char* message) {
  GLenum err_code = glGetError();
  if (err_code != GL_NO_ERROR) {
    ALOGE("GL Error Found (%d): %s", err_code, message);
  }
}

void shader_init(const GLchar** v_src,
                 const GLchar** f_src,
                 GLint* v_size,
                 GLint* f_size) {
  shader_prog_id = glCreateProgram();
  GLuint v_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint f_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(v_shader, 1, v_src, v_size);
  glShaderSource(f_shader, 1, f_src, f_size);
  glCompileShader(v_shader);
  glCompileShader(f_shader);
  glAttachShader(shader_prog_id, v_shader);
  glAttachShader(shader_prog_id, f_shader);
  glLinkProgram(shader_prog_id);
  glValidateProgram(shader_prog_id);
  GLint result;
  glGetProgramiv(shader_prog_id, GL_VALIDATE_STATUS, &result);
  ALOGE("Program Valid: %d", result);
  glCheckError("Shader init");
}

void vbo_init() {
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data),
               g_vertex_buffer_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glCheckError("VBO init");
}

void GetRGBAImageOfTexture(GLuint texture_id,
                           int width,
                           int height,
                           GLuint data_type,
                           void* buffer) {
  // glBindTexture(GL_TEXTURE_2D, texture_id);

  GLuint FramebufferName = 0;
  glGenFramebuffers(1, &FramebufferName);
  glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_id, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status == GL_FRAMEBUFFER_COMPLETE) {
    // glUseProgram(shader_prog_id);

    /*glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    GLuint vertexHandle = 0;
    glEnableVertexAttribArray(vertexHandle);
    glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, 0);*/

    /*glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glCheckError("Draw Array");*/
    glReadPixels(0, 0, width, height, GL_RGBA, data_type, buffer);
    glCheckError("Read Pixels");

    // glDisableVertexAttribArray(vertexHandle);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &FramebufferName);
}

JNI_FUNCTION_DECLARATION(jint,
                         AIMatter_nativeTestJniFunc,
                         JNIEnv* jni,
                         jobject this_object,
                         jint input) {
  return input + 1;
}

JNI_FUNCTION_DECLARATION(jlong,
                         AIMatter_nativeInitFabbyVideoSegmenter,
                         JNIEnv* jni,
                         jobject this_object,
                         jobject asset_manager,
                         jstring asset_path) {
  AAssetManager* mgr = AAssetManager_fromJava(jni, asset_manager);
  const char* path = jni->GetStringUTFChars(asset_path, 0);
  FabbySDKVideoSegmenterInfo* info = new FabbySDKVideoSegmenterInfo();

  FabbySDKControlFunctions funcs;
  funcs.progress_fn = nullptr;
  funcs.progress_data = nullptr;
  funcs.cancel_fn = nullptr;
  funcs.cancel_data = nullptr;

  int result = FabbySDKInitVideoSegmenterFromAsset(mgr, path, info, &funcs);
  if (result != FABBY_SDK_SUCCESS) {
    delete info;
    info = nullptr;
  }

  GLint vsrc_size = strlen(vert_src);
  GLint fsrc_size = strlen(frag_src);
  vbo_init();
  shader_init(&vert_src, &frag_src, &vsrc_size, &fsrc_size);

  return reinterpret_cast<jlong>(info);
}

JNI_FUNCTION_DECLARATION(void,
                         AIMatter_nativeDestroyFabbyVideoSegmenter,
                         JNIEnv* jni,
                         jobject this_object,
                         jlong info_ptr) {
  FabbySDKVideoSegmenterInfo* info =
      reinterpret_cast<FabbySDKVideoSegmenterInfo*>(info_ptr);
  if (!info)
    return;
  FabbySDKDestroyVideoSegmenter(info->handle);
  delete info;
}

JNI_FUNCTION_DECLARATION(jint,
                         AIMatter_nativeFabbyVideoSegment,
                         JNIEnv* jni,
                         jobject this_object,
                         jlong info_ptr,
                         jint texture_id,
                         jint width,
                         jint height,
                         jint camera_angle,
                         jint camera_facing,
                         jbyteArray mask) {
  // Debug Code For Alpha Channel Code Path
  // TODO(qiangchen): Call FabbySDK API to do real segmentation
  FABBY_SDK_RESULT result = FABBY_SDK_SUCCESS;
  jbyte* mask_array = jni->GetByteArrayElements(mask, nullptr);

  FabbySDKVideoSegmenterInfo* info =
      reinterpret_cast<FabbySDKVideoSegmenterInfo*>(info_ptr);

  FabbySDKControlFunctions funcs;
  funcs.progress_fn = nullptr;
  funcs.progress_data = nullptr;
  funcs.cancel_fn = nullptr;
  funcs.cancel_data = nullptr;

  FabbySDKTexture texture;
  texture.type = 0;
  texture.width = width;
  texture.height = height;
  texture.textureId = texture_id;

  FabbySDKTexture rotatedTexture;
  FabbySDKTexture rotatedMask;

  result = FabbySDKVideoSegmentObject(info->handle, texture, camera_angle,
                                      camera_facing, &rotatedTexture,
                                      &rotatedMask, &funcs);

  std::unique_ptr<short[]> mask_buffer(
      new short[rotatedMask.width * rotatedMask.height * 4]);

  GetRGBAImageOfTexture(rotatedMask.textureId, rotatedMask.width,
                        rotatedMask.height, GL_HALF_FLOAT, mask_buffer.get());

  std::unique_ptr<int[]> source_x(new int[width]);
  for (int x = 0; x < width; x++) {
    source_x[x] = x * rotatedMask.width / width;
  }

  int target_index = 0;
  for (int y = 0; y < height; y++) {
    int source_y = y * rotatedMask.height / height;

    for (int x = 0; x < width; x++, target_index++) {
      int source_index = source_y * rotatedMask.width + source_x[x];

      /*if (camera_angle == 0) {
        int source_y = y*rotatedMask.height / height;
        int source_x = x*rotatedMask.width / width;
        source_index = source_y*rotatedMask.width + source_x;
      } else if (camera_angle == 180) {
        int source_y = (rotatedMask.height - 1) - y*rotatedMask.height / height;
        int source_x = (rotatedMask.width - 1) - x*rotatedMask.width / width;
        source_index = source_y*rotatedMask.width + source_x;
      } else if (camera_angle == 90) {
        int source_y =  x*rotatedMask.height / width;
        int source_x = (rotatedMask.width - 1) - y*rotatedMask.width / height;
        source_index = source_y*rotatedMask.width + source_x;
      } else if (camera_angle == 270) {
        int source_y = (rotatedMask.height - 1) - x*rotatedMask.height / width;
        int source_x = y*rotatedMask.width / height;
        source_index = source_y*rotatedMask.width + source_x;
      } else {
        ALOGE("Unknown Camera Angle. %d", camera_angle);
      }*/

      /*float probability =
      Float16Compressor::decompress(mask_buffer[source_index * 4]); if
      (probability < 0.2) mask_array[target_index] = 0; else
        mask_array[target_index] = (unsigned char)(probability * 255);*/

      // By Half Float Definition, probability = (1024+w)*2^(e - 25)
      int probability = mask_buffer[source_index * 4];
      int e = (probability & 0x3C00) >> 10;
      int w = (probability & 0x03FF);
      if (e >= 15) {
        mask_array[target_index] = 0xFF;
      } else if (e < 11) {
        mask_array[target_index] = 0;
      } else {
        mask_array[target_index] = (jbyte)((1024 | w) >> (17 - e));
      }
    }
  }

  // ALOGE("Qiang Chen: Input Angle: %d, Rotated Mask Size: %dx%d,
  // RotatedTexture Size: %dx%d, Needed Size: %dx%d",
  //               camera_angle, rotatedMask.width, rotatedMask.height,
  //               rotatedTexture.width, rotatedTexture.height,
  //               width, height);

  jni->ReleaseByteArrayElements(mask, mask_array, 0);
  return result;
}

JNI_FUNCTION_DECLARATION(jint,
                         AIMatter_nativeFabbyVideoSegment2,
                         JNIEnv* jni,
                         jobject this_object,
                         jlong info_ptr,
                         jint texture_id,
                         jint width,
                         jint height,
                         jint camera_angle,
                         jint camera_facing,
                         jintArray mask) {
  // Debug Code For Alpha Channel Code Path
  // TODO(qiangchen): Call FabbySDK API to do real segmentation
  FABBY_SDK_RESULT result = FABBY_SDK_SUCCESS;
  jint* mask_array = jni->GetIntArrayElements(mask, nullptr);

  FabbySDKVideoSegmenterInfo* info =
      reinterpret_cast<FabbySDKVideoSegmenterInfo*>(info_ptr);

  FabbySDKControlFunctions funcs;
  funcs.progress_fn = nullptr;
  funcs.progress_data = nullptr;
  funcs.cancel_fn = nullptr;
  funcs.cancel_data = nullptr;

  FabbySDKTexture texture;
  texture.type = 0;
  texture.width = width;
  texture.height = height;
  texture.textureId = texture_id;

  FabbySDKTexture rotatedTexture;
  FabbySDKTexture rotatedMask;

  result = FabbySDKVideoSegmentObject(info->handle, texture, camera_angle,
                                      camera_facing, &rotatedTexture,
                                      &rotatedMask, &funcs);

  mask_array[0] = rotatedMask.textureId;
  mask_array[1] = rotatedMask.width;
  mask_array[2] = rotatedMask.height;

  jni->ReleaseIntArrayElements(mask, mask_array, 0);
  return result;
}

}  // namespace jni
}  // namespace AppRTCMobile
