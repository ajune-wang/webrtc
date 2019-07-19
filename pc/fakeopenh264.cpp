/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(_WIN32)
#include "windows.h"
#elif defined(__APPLE__) && (__MACH__) || (__linux__)
#include <dlfcn.h>
#endif
#include "pc/fakeopenh264.h"
#include "rtc_base/logging.h"

bool loaded;
#if defined(_WIN32)
HINSTANCE hinstLib;
#elif defined(__APPLE__) && (__MACH__) || (__linux__)
void* openlib;
#endif
CreateH264Encoder pCreateEnc;
CreateH264Decoder pCreateDec;
DestroyH264Encoder pDestroyEnc;
DestroyH264Decoder pDestroyDec;
const char* libpath;

void setLibPath() {
#if defined(_WIN32)
  if (sizeof(void*) == 8) {
    libpath = "openh264-1.8.0-win64.dll";
  }
  if (sizeof(void*) == 4) {
    libpath = "openh264-1.8.0-win32.dll";
  }
#elif defined(__APPLE__) && (__MACH__)
  if (sizeof(void*) == 8) {
    libpath = "./libopenh264-1.8.0-osx64.4.dylib";
  }
  if (sizeof(void*) == 4) {
    libpath = "./libopenh264-1.8.0-osx32.4.dylib";
  }

#elif defined(__linux__)
  if (sizeof(void*) == 8) {
    libpath = "./libopenh264-1.8.0-linux64.4.so";
  }
  if (sizeof(void*) == 4) {
    libpath = "./libopenh264-1.8.0-linux32.4.so";
  }
#endif
}

// encoding functions
int WelsCreateSVCEncoder(ISVCEncoder** ppEncoder) {
  RTC_LOG(LS_INFO) << " PPEncoder " << ppEncoder;
  if (!loaded) {
    loadLib();
  }

  return pCreateEnc(ppEncoder);
}

void WelsDestroySVCEncoder(ISVCEncoder* pEncoder) {
  if (!loaded) {
    loadLib();
  }
  pDestroyEnc(pEncoder);
}
// decoding functions
long WelsCreateDecoder(ISVCDecoder** ppDecoder) {
  if (!loaded) {
    loadLib();
  }
  return pCreateDec(ppDecoder);
}

void WelsDestroyDecoder(ISVCDecoder* pDecoder) {
  if (!loaded) {
    loadLib();
  }

  pDestroyDec(pDecoder);
}

void loadLib() {
  setLibPath();
#if defined(_WIN32)
  hinstLib = LoadLibraryA(libpath);
  RTC_LOG(LS_INFO) << "Lib PATH:: " << libpath;
  pCreateEnc =
      (CreateH264Encoder)GetProcAddress(hinstLib, "WelsCreateSVCEncoder");
  pDestroyEnc =
      (DestroyH264Encoder)GetProcAddress(hinstLib, "WelsDestroySVCEncoder");
  pCreateDec = (CreateH264Decoder)GetProcAddress(hinstLib, "WelsCreateDecoder");
  pDestroyDec =
      (DestroyH264Decoder)GetProcAddress(hinstLib, "WelsDestroyDecoder");
  if (!hinstLib || !pCreateEnc) {
    loaded = false;
    RTC_LOG(LS_INFO) << "OPENH264 lib not loaded";
  } else {
    loaded = true;
    RTC_LOG(LS_INFO) << "OPENH264 lib loaded";
  }
#elif defined(__APPLE__) && (__MACH__) || (__linux__)
  openlib = dlopen(libpath, RTLD_LAZY);
  pCreateEnc = (CreateH264Encoder)dlsym(openlib, "WelsCreateSVCEncoder");
  pDestroyEnc = (DestroyH264Encoder)dlsym(openlib, "WelsDestroySVCEncoder");
  pCreateDec = (CreateH264Decoder)dlsym(openlib, "WelsCreateDecoder");
  pDestroyDec = (DestroyH264Decoder)dlsym(openlib, "WelsDestroyDecoder");
  if (!openlib || !pCreateEnc) {
    loaded = false;
    RTC_LOG(LS_INFO) << "OPENH264 lib not loaded";
  } else {
    loaded = true;
    RTC_LOG(LS_INFO) << "OPENH264 lib loaded";
  }
#endif
}

void closeLib() {
#if defined(_WIN32)
  FreeLibrary(hinstLib);
#elif defined(__APPLE__) && (__MACH__) || (__linux__)
  dlclose(openlib);
#endif
}

bool amIloaded() {
  return loaded;
}
