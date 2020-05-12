cmake_minimum_required(VERSION 3.14)

project(webrtc_vad)

add_executable(webrtc_vad ${CMAKE_SOURCE_DIR}/main.cpp)

target_sources(webrtc_vad
	PRIVATE
		${CMAKE_SOURCE_DIR}/common_audio/vad/webrtc_vad.c
		${CMAKE_SOURCE_DIR}/common_audio/vad/vad_core.c
		${CMAKE_SOURCE_DIR}/common_audio/vad/vad_filterbank.c
		${CMAKE_SOURCE_DIR}/common_audio/vad/vad_gmm.c
		${CMAKE_SOURCE_DIR}/common_audio/vad/vad_sp.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/division_operations.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/resample_48khz.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/spl_inl.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/energy.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/resample_fractional.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/resample_by_2_internal.c
		${CMAKE_SOURCE_DIR}/common_audio/signal_processing/get_scaling_square.c
		${CMAKE_SOURCE_DIR}/common_audio/wav_file.cc
		${CMAKE_SOURCE_DIR}/common_audio/wav_header.cc
		${CMAKE_SOURCE_DIR}/rtc_base/checks.cc
		${CMAKE_SOURCE_DIR}/rtc_base/system/file_wrapper.cc
)

target_compile_definitions(webrtc_vad PRIVATE WEBRTC_WIN NOMINMAX RTC_DISABLE_CHECK_MSG RTC_DISABLE_LOGGING)

target_include_directories(webrtc_vad
	PUBLIC
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/../abseil-cpp
	)