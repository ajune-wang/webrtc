/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RTC_EXPORT_H_
#define RTC_BASE_RTC_EXPORT_H_

// This is a redacted version of the Chromium's base/component_export.h
// available here (it renames macros adding the RTC_ prefix and removing
// COMPONENT): https://cs.chromium.org/chromium/src/base/component_export.h

// Used to annotate symbols which are exported by the component named
// |component|. Note that this only does the right thing if the corresponding
// component target's sources are compiled with |IS_$component_IMPL| defined
// as 1. For example:
//
//   class RTC_EXPORT(FOO) Bar {};
//
// If IS_FOO_IMPL=1 at compile time, then Bar will be annotated using the
// RTC_EXPORT_ANNOTATION macro defined below. Otherwise it will be
// annotated using the COMPONENT_IMPORT_ANNOTATION macro.
#define RTC_EXPORT(component)                         \
  RTC_MACRO_CONDITIONAL_(IS_##component##_IMPL,       \
                         RTC_EXPORT_ANNOTATION,       \
                         RTC_IMPORT_ANNOTATION)

// Indicates whether the current compilation unit is being compiled as part of
// the implementation of the component named |component|. Expands to |1| if
// |IS_$component_IMPL| is defined as |1|; expands to |0| otherwise.
//
// Note in particular that if |IS_$component_IMPL| is not defined at all, it is
// still fine to test INSIDE_COMPONENT_IMPL(component), which expands to |0| as
// expected.
#define RTC_INSIDE_IMPL(component) \
  RTC_MACRO_CONDITIONAL_(IS_##component##_IMPL, 1, 0)

// Compiler-specific macros to annotate for export or import of a symbol. No-op
// in non-component builds. These should not see much if any direct use.
// Instead use the COMPONENT_EXPORT macro defined above.
#if defined(COMPONENT_BUILD)
#if defined(WIN32)
#define RTC_EXPORT_ANNOTATION __declspec(dllexport)
#define RTC_IMPORT_ANNOTATION __declspec(dllimport)
#else  // defined(WIN32)
#define RTC_EXPORT_ANNOTATION __attribute__((visibility("default")))
#define RTC_IMPORT_ANNOTATION
#endif  // defined(WIN32)
#else   // defined(COMPONENT_BUILD)
#define RTC_EXPORT_ANNOTATION
#define RTC_IMPORT_ANNOTATION
#endif  // defined(COMPONENT_BUILD)

// Below this point are several internal utility macros used for the
// implementation of the above macros. Not intended for external use.

// Helper for conditional expansion to one of two token strings. If |condition|
// expands to |1| then this macro expands to |consequent|; otherwise it expands
// to |alternate|.
#define RTC_MACRO_CONDITIONAL_(condition, consequent, alternate) \
  RTC_MACRO_SELECT_THIRD_ARGUMENT_(                              \
      RTC_MACRO_CONDITIONAL_COMMA_(condition), consequent, alternate)

// Expands to a comma (,) iff its first argument expands to |1|. Used in
// conjunction with |RTC_MACRO_SELECT_THIRD_ARGUMENT_()|, as the presence
// or absense of an extra comma can be used to conditionally shift subsequent
// argument positions and thus influence which argument is selected.
#define RTC_MACRO_CONDITIONAL_COMMA_(...) \
  RTC_MACRO_CONDITIONAL_COMMA_IMPL_(__VA_ARGS__, )
#define RTC_MACRO_CONDITIONAL_COMMA_IMPL_(x, ...) \
  RTC_MACRO_CONDITIONAL_COMMA_##x##_
#define RTC_MACRO_CONDITIONAL_COMMA_1_ ,

// Helper which simply selects its third argument. Used in conjunction with
// |RTC_MACRO_CONDITIONAL_COMMA_()| above to implement conditional macro
// expansion.
#define RTC_MACRO_SELECT_THIRD_ARGUMENT_(...) \
  RTC_MACRO_EXPAND_(                          \
      RTC_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(__VA_ARGS__))
#define RTC_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(a, b, c, ...) c

// Helper to work around MSVC quirkiness wherein a macro expansion like |,|
// within a parameter list will be treated as a single macro argument. This is
// needed to ensure that |RTC_MACRO_CONDITIONAL_COMMA_()| above can expand
// to multiple separate positional arguments in the affirmative case, thus
// elliciting the desired conditional behavior with
// |RTC_MACRO_SELECT_THIRD_ARGUMENT_()|.
#define RTC_MACRO_EXPAND_(x) x

#endif  // RTC_BASE_RTC_EXPORT_H_
