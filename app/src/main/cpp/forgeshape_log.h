// The native core's ONE logging seam (`DESKTOP-R1`).
//
// On Android every line goes to logcat under the caller's tag, exactly as the
// direct `__android_log_print` calls it replaces did, so every token a test or
// a CI script greps for (`FORGESHAPE_NATIVE_VIEWPORT_OK`, the self-test tokens)
// reads the same. On the desktop the same line goes to stderr, prefixed by the
// tag, where the shell and the automated checks read it.
//
// Deliberately two macros and nothing else: no levels beyond info and error,
// no sinks, no formatting of its own.
#pragma once

#if defined(__ANDROID__)

#include <android/log.h>

#define FS_LOG_INFO(tag, ...) __android_log_print(ANDROID_LOG_INFO, tag, __VA_ARGS__)
#define FS_LOG_ERROR(tag, ...) __android_log_print(ANDROID_LOG_ERROR, tag, __VA_ARGS__)

#else

#include <cstdarg>
#include <cstdio>

namespace forgeshape {

inline void desktopLog(const char* level, const char* tag, const char* format, ...) {
    std::fprintf(stderr, "%s %s: ", level, tag);
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

}  // namespace forgeshape

#define FS_LOG_INFO(tag, ...) ::forgeshape::desktopLog("I", tag, __VA_ARGS__)
#define FS_LOG_ERROR(tag, ...) ::forgeshape::desktopLog("E", tag, __VA_ARGS__)

#endif
