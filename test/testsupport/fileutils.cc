/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fileutils.h"

#include <assert.h>

#ifdef WEBRTC_POSIX
#include <unistd.h>
#endif

#ifdef WIN32
#include <direct.h>
#include <tchar.h>
#include <windows.h>
#include <algorithm>
#include <locale>

#include "Shlwapi.h"
#include "WinDef.h"

#include "rtc_base/win32.h"
#define GET_CURRENT_DIR _getcwd
#else
#include <dirent.h>
#include <unistd.h>

#define GET_CURRENT_DIR getcwd
#endif

#include <sys/stat.h>  // To check for directory existence.
#ifndef S_ISDIR        // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_IOS)
// Defined in iosfileutils.mm.  No header file to discourage use elsewhere.
std::string IOSOutputPath();
std::string IOSRootPath();
std::string IOSResourcePath(std::string name, std::string extension);
#endif

namespace {

#ifdef WIN32
const char* kPathDelimiter = "\\";
const wchar_t* kPathDelimiterW = L"\\";

std::string WideStringToString(const std::wstring& wstr) {
  using convert_type = std::codecvt_utf8<wchar_t>;
  std::wstring_convert<convert_type, wchar_t> converter;

  return converter.to_bytes(wstr);
}
#else
const char* kPathDelimiter = "/";
const wchar_t* kPathDelimiterW = L"/";
#endif

#ifdef WEBRTC_ANDROID
const char* kRootDirName = "/sdcard/chromium_tests_root/";
#else
#if !defined(WEBRTC_IOS)
const char* kOutputDirName = "out";
#endif
const char* kFallbackPath = "./";
#endif  // !defined(WEBRTC_ANDROID)

#if !defined(WEBRTC_IOS)
const char* kResourcesDirName = "resources";
#endif

char relative_dir_path[FILENAME_MAX];
bool relative_dir_path_set = false;

}  // namespace

const char* kCannotFindProjectRootDir = "ERROR_CANNOT_FIND_PROJECT_ROOT_DIR";

std::string DirName(const std::string path) {
  return path.substr(0, path.find_last_of(kPathDelimiter));
}

std::wstring DirName(const std::wstring path) {
  return path.substr(0, path.find_last_of(kPathDelimiterW));
}

void SetExecutablePath(const std::string& path) {
  std::string working_dir = WorkingDir();
  std::string temp_path = path;

  // Handle absolute paths; convert them to relative paths to the working dir.
  if (path.find(working_dir) != std::string::npos) {
    temp_path = path.substr(working_dir.length() + 1);
  }
// On Windows, when tests are run under memory tools like DrMemory and TSan,
// slashes occur in the path as directory separators. Make sure we replace
// such cases with backslashes in order for the paths to be correct.
#ifdef WIN32
  std::replace(temp_path.begin(), temp_path.end(), '/', '\\');
#endif

  // Trim away the executable name; only store the relative dir path.
  temp_path = DirName(temp_path);
  strncpy(relative_dir_path, temp_path.c_str(), FILENAME_MAX);
  relative_dir_path_set = true;
}

bool FileExists(const std::string& file_name) {
  struct stat file_info = {0};
  return stat(file_name.c_str(), &file_info) == 0;
}

bool DirExists(const std::string& directory_name) {
  struct stat directory_info = {0};
  return stat(directory_name.c_str(), &directory_info) == 0 &&
         S_ISDIR(directory_info.st_mode);
}

#ifdef WEBRTC_ANDROID

std::string ProjectRootPath() {
  return kRootDirName;
}

std::string OutputPath() {
  return kRootDirName;
}

std::string WorkingDir() {
  return kRootDirName;
}

#else  // WEBRTC_ANDROID

std::string ProjectRootPath() {
#if defined WEBRTC_LINUX
  char buf[PATH_MAX];
  ssize_t count = ::readlink("/proc/self/exe", buf, arraysize(buf));
  if (count <= 0) {
    RTC_NOTREACHED() << "Unable to resolve /proc/self/exe.";
    return kCannotFindProjectRootDir;
  }
  // On POSIX, tests execute in out/Whatever, so src is two levels up.
  std::string exe_dir = DirName(std::string(buf, count));
  return DirName(DirName(exe_dir)) + kPathDelimiter;
#elif defined WIN32
  wchar_t buf[MAX_PATH];
  buf[0] = 0;
  if (GetModuleFileName(NULL, buf, MAX_PATH) == 0)
    return kCannotFindProjectRootDir;
  std::wstring exe_dir = DirName(std::wstring(buf, count));
  std::wstring src_dir = DirName(DirName(exe_dir)) + kPathDelimiterW;
  return WideStringToString(src_dir);
#elif defined WEBRTC_ANDROID
  return "/sdcard/chromium_tests_root/";
#elif defined WEBRTC_IOS
  return IOSRootPath();
#endif
}

std::string OutputPath() {
#if defined(WEBRTC_IOS)
  return IOSOutputPath();
#else
  std::string path = ProjectRootPath();
  if (path == kCannotFindProjectRootDir) {
    return kFallbackPath;
  }
  path += kOutputDirName;
  if (!CreateDir(path)) {
    return kFallbackPath;
  }
  return path + kPathDelimiter;
#endif
}

std::string WorkingDir() {
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return kFallbackPath;
  } else {
    return std::string(path_buffer);
  }
}

#endif  // !WEBRTC_ANDROID

// Generate a temporary filename in a safe way.
// Largely copied from talk/base/{unixfilesystem,win32filesystem}.cc.
std::string TempFilename(const std::string& dir, const std::string& prefix) {
#ifdef WIN32
  wchar_t filename[MAX_PATH];
  if (::GetTempFileName(rtc::ToUtf16(dir).c_str(), rtc::ToUtf16(prefix).c_str(),
                        0, filename) != 0)
    return rtc::ToUtf8(filename);
  assert(false);
  return "";
#else
  int len = dir.size() + prefix.size() + 2 + 6;
  std::unique_ptr<char[]> tempname(new char[len]);

  snprintf(tempname.get(), len, "%s/%sXXXXXX", dir.c_str(), prefix.c_str());
  int fd = ::mkstemp(tempname.get());
  if (fd == -1) {
    assert(false);
    return "";
  } else {
    ::close(fd);
  }
  std::string ret(tempname.get());
  return ret;
#endif
}

std::string GenerateTempFilename(const std::string& dir,
                                 const std::string& prefix) {
  std::string filename = TempFilename(dir, prefix);
  RemoveFile(filename);
  return filename;
}

absl::optional<std::vector<std::string>> ReadDirectory(std::string path) {
  if (path.length() == 0)
    return absl::optional<std::vector<std::string>>();

#if defined(WEBRTC_WIN)
  // Append separator character if needed.
  if (path.back() != '\\')
    path += '\\';

  // Init.
  WIN32_FIND_DATA data;
  HANDLE handle = ::FindFirstFile(rtc::ToUtf16(path + '*').c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE)
    return absl::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  do {
    const std::string name = rtc::ToUtf8(data.cFileName);
    if (name != "." && name != "..")
      found_entries.emplace_back(path + name);
  } while (::FindNextFile(handle, &data) == TRUE);

  // Release resources.
  if (handle != INVALID_HANDLE_VALUE)
    ::FindClose(handle);
#else
  // Append separator character if needed.
  if (path.back() != '/')
    path += '/';

  // Init.
  DIR* dir = ::opendir(path.c_str());
  if (dir == nullptr)
    return absl::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  while (dirent* dirent = readdir(dir)) {
    const std::string& name = dirent->d_name;
    if (name != "." && name != "..")
      found_entries.emplace_back(path + name);
  }

  // Release resources.
  closedir(dir);
#endif

  return absl::optional<std::vector<std::string>>(std::move(found_entries));
}

bool CreateDir(const std::string& directory_name) {
  struct stat path_info = {0};
  // Check if the path exists already:
  if (stat(directory_name.c_str(), &path_info) == 0) {
    if (!S_ISDIR(path_info.st_mode)) {
      fprintf(stderr,
              "Path %s exists but is not a directory! Remove this "
              "file and re-run to create the directory.\n",
              directory_name.c_str());
      return false;
    }
  } else {
#ifdef WIN32
    return _mkdir(directory_name.c_str()) == 0;
#else
    return mkdir(directory_name.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
  }
  return true;
}

bool RemoveDir(const std::string& directory_name) {
#ifdef WIN32
  return RemoveDirectoryA(directory_name.c_str()) != FALSE;
#else
  return rmdir(directory_name.c_str()) == 0;
#endif
}

bool RemoveFile(const std::string& file_name) {
#ifdef WIN32
  return DeleteFileA(file_name.c_str()) != FALSE;
#else
  return unlink(file_name.c_str()) == 0;
#endif
}

std::string ResourcePath(const std::string& name,
                         const std::string& extension) {
#if defined(WEBRTC_IOS)
  return IOSResourcePath(name, extension);
#else
  std::string platform = "win";
#ifdef WEBRTC_LINUX
  platform = "linux";
#endif  // WEBRTC_LINUX
#ifdef WEBRTC_MAC
  platform = "mac";
#endif  // WEBRTC_MAC
#ifdef WEBRTC_ANDROID
  platform = "android";
#endif  // WEBRTC_ANDROID

  std::string resources_path =
      ProjectRootPath() + kResourcesDirName + kPathDelimiter;
  std::string resource_file =
      resources_path + name + "_" + platform + "." + extension;
  if (FileExists(resource_file)) {
    return resource_file;
  }
  // Fall back on name without platform.
  return resources_path + name + "." + extension;
#endif  // defined (WEBRTC_IOS)
}

std::string JoinFilename(const std::string& dir, const std::string& name) {
  RTC_CHECK(!dir.empty()) << "Special cases not implemented.";
  return dir + kPathDelimiter + name;
}

size_t GetFileSize(const std::string& filename) {
  FILE* f = fopen(filename.c_str(), "rb");
  size_t size = 0;
  if (f != NULL) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

}  // namespace test
}  // namespace webrtc
