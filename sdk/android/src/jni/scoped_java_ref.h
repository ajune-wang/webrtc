/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Originally these classes are from Chromium.
// https://cs.chromium.org/chromium/src/base/android/scoped_java_ref.h.

#ifndef SDK_ANDROID_SRC_JNI_SCOPED_JAVA_REF_H_
#define SDK_ANDROID_SRC_JNI_SCOPED_JAVA_REF_H_

#include <jni.h>
#include <cstddef>

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace jni {

// Forward declare the generic java reference template class.
template <typename T>
class JavaRef;

// Template specialization of JavaRef, which acts as the base class for all
// other JavaRef<> template types. This allows you to e.g. pass
// ScopedJavaLocalRef<jstring> into a function taking const JavaRef<jobject>&
template <>
class JavaRef<jobject> {
 public:
  // Initializes a null reference. Don't add anything else here; it's inlined.
  JavaRef() : obj_(nullptr) {}

  // Allow nullptr to be converted to JavaRef. This avoids having to declare an
  // empty JavaRef just to pass null to a function, and makes C++ "nullptr" and
  // Java "null" equivalent.
  JavaRef(std::nullptr_t) : JavaRef() {}  // NOLINT(runtime/explicit)

  // Public to allow destruction of null JavaRef objects.
  // Don't add anything else here; it's inlined.
  ~JavaRef() {}

  jobject obj() const { return obj_; }

  bool is_null() const { return obj_ == nullptr; }

 protected:
// Takes ownership of the |obj| reference passed; requires it to be a local
// reference type.
#if RTC_DCHECK_IS_ON
  // Implementation contains a DCHECK; implement out-of-line when DCHECK_IS_ON.
  JavaRef(JNIEnv* env, jobject obj);
#else
  // Don't add anything else here; it's inlined.
  JavaRef(JNIEnv* env, jobject obj) : obj_(obj) {}
#endif

  void swap(JavaRef& other) { std::swap(obj_, other.obj_); }

  // The following are implementation detail convenience methods, for
  // use by the sub-classes.
  JNIEnv* SetNewLocalRef(JNIEnv* env, jobject obj);
  void SetNewGlobalRef(JNIEnv* env, jobject obj);
  void ResetLocalRef(JNIEnv* env);
  void ResetGlobalRef();
  jobject ReleaseInternal();

 private:
  jobject obj_;

  RTC_DISALLOW_COPY_AND_ASSIGN(JavaRef);
};

// Generic base class for ScopedJavaLocalRef and ScopedJavaGlobalRef. Useful
// for allowing functions to accept a reference without having to mandate
// whether it is a local or global type.
template <typename T>
class JavaRef : public JavaRef<jobject> {
 public:
  JavaRef() {}
  JavaRef(std::nullptr_t)  // NOLINT(runtime/explicit)
      : JavaRef<jobject>(nullptr) {}
  ~JavaRef() {}

  T obj() const { return static_cast<T>(JavaRef<jobject>::obj()); }

 protected:
  JavaRef(JNIEnv* env, T obj) : JavaRef<jobject>(env, obj) {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(JavaRef);
};

// Holds a local reference to a JNI method parameter.
// Method parameters should not be deleted, and so this class exists purely to
// wrap them as a JavaRef<T> in the JNI binding generator. Do not create
// instances manually.
template <typename T>
class JavaParamRef : public JavaRef<T> {
 public:
  // Assumes that |obj| is a parameter passed to a JNI method from Java.
  // Does not assume ownership as parameters should not be deleted.
  JavaParamRef(JNIEnv* env, T obj) : JavaRef<T>(env, obj) {}

  // Allow nullptr to be converted to JavaParamRef. Some unit tests call JNI
  // methods directly from C++ and pass null for objects which are not actually
  // used by the implementation (e.g. the caller object); allow this to keep
  // working.
  JavaParamRef(std::nullptr_t)  // NOLINT(runtime/explicit)
      : JavaRef<T>(nullptr) {}

  ~JavaParamRef() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(JavaParamRef);
};

// Holds a local reference to a Java object. The local reference is scoped
// to the lifetime of this object.
// Instances of this class may hold onto any JNIEnv passed into it until
// destroyed. Therefore, since a JNIEnv is only suitable for use on a single
// thread, objects of this class must be created, used, and destroyed, on a
// single thread.
// Therefore, this class should only be used as a stack-based object and from a
// single thread. If you wish to have the reference outlive the current
// callstack (e.g. as a class member) or you wish to pass it across threads,
// use a ScopedJavaGlobalRef instead.
template <typename T>
class ScopedJavaLocalRef : public JavaRef<T> {
 public:
  ScopedJavaLocalRef() : env_(nullptr) {}
  ScopedJavaLocalRef(std::nullptr_t)  // NOLINT(runtime/explicit)
      : env_(nullptr) {}

  // Non-explicit copy constructor, to allow ScopedJavaLocalRef to be returned
  // by value as this is the normal usage pattern.
  ScopedJavaLocalRef(const ScopedJavaLocalRef<T>& other) : env_(other.env_) {
    this->SetNewLocalRef(env_, other.obj());
  }

  template <typename G>
  ScopedJavaLocalRef(ScopedJavaLocalRef<G>&& other) {
    this->swap(other);
  }

  explicit ScopedJavaLocalRef(const JavaRef<T>& other) : env_(nullptr) {
    this->Reset(other);
  }

  // Assumes that |obj| is a local reference to a Java object and takes
  // ownership  of this local reference. This should preferably not be used
  // outside of JNI helper functions.
  ScopedJavaLocalRef(JNIEnv* env, T obj) : JavaRef<T>(env, obj), env_(env) {}

  ~ScopedJavaLocalRef() { this->Reset(); }

  // Overloaded assignment operator defined for consistency with the implicit
  // copy constructor.
  void operator=(const ScopedJavaLocalRef<T>& other) { this->Reset(other); }

  void operator=(ScopedJavaLocalRef<T>&& other) {
    env_ = other.env_;
    this->swap(other);
  }

  void Reset() { this->ResetLocalRef(env_); }

  void Reset(const ScopedJavaLocalRef<T>& other) {
    // We can copy over env_ here as |other| instance must be from the same
    // thread as |this| local ref. (See class comment for multi-threading
    // limitations, and alternatives).
    this->Reset(other.env_, other.obj());
  }

  void Reset(const JavaRef<T>& other) {
    // If |env_| was not yet set (is still null) it will be attached to the
    // current thread in SetNewLocalRef().
    this->Reset(env_, other.obj());
  }

  // Creates a new local reference to the Java object, unlike the constructor
  // with the same parameters that takes ownership of the existing reference.
  void Reset(JNIEnv* env, T obj) { env_ = this->SetNewLocalRef(env, obj); }

  // Releases the local reference to the caller. The caller *must* delete the
  // local reference when it is done with it. Note that calling a Java method
  // is *not* a transfer of ownership and Release() should not be used.
  T Release() { return static_cast<T>(this->ReleaseInternal()); }

 private:
  // This class is only good for use on the thread it was created on so
  // it's safe to cache the non-threadsafe JNIEnv* inside this object.
  JNIEnv* env_;
};

// Holds a global reference to a Java object. The global reference is scoped
// to the lifetime of this object. This class does not hold onto any JNIEnv*
// passed to it, hence it is safe to use across threads (within the constraints
// imposed by the underlying Java object that it references).
template <typename T>
class ScopedJavaGlobalRef : public JavaRef<T> {
 public:
  ScopedJavaGlobalRef() {}
  ScopedJavaGlobalRef(std::nullptr_t) {}  // NOLINT(runtime/explicit)

  ScopedJavaGlobalRef(ScopedJavaGlobalRef<T>&& other) { this->swap(other); }

  ScopedJavaGlobalRef(JNIEnv* env, T obj) { this->Reset(env, obj); }
  ScopedJavaGlobalRef(JNIEnv* env, const JavaRef<T>& other) {
    this->Reset(env, other.obj());
  }

  explicit ScopedJavaGlobalRef(const JavaRef<T>& other) { this->Reset(other); }

  ~ScopedJavaGlobalRef() { this->Reset(); }

  void operator=(ScopedJavaGlobalRef<T>&& other) { this->swap(other); }

  void Reset() { this->ResetGlobalRef(); }

  void Reset(const JavaRef<T>& other) { this->Reset(nullptr, other.obj()); }

  void Reset(JNIEnv* env, T obj) { this->SetNewGlobalRef(env, obj); }

  // Releases the global reference to the caller. The caller *must* delete the
  // global reference when it is done with it. Note that calling a Java method
  // is *not* a transfer of ownership and Release() should not be used.
  T Release() { return static_cast<T>(this->ReleaseInternal()); }

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(ScopedJavaGlobalRef);
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_SCOPED_JAVA_REF_H_
