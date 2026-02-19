#pragma once

/// Cross-platform logging macros for the typesetting engine.
/// Android: uses __android_log_print
/// Other platforms: uses fprintf(stderr, ...)

#ifdef __ANDROID__

#include <android/log.h>

#define TS_LOG_TAG "Typesetting"
#define TS_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TS_LOG_TAG, __VA_ARGS__)
#define TS_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TS_LOG_TAG, __VA_ARGS__)
#define TS_LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TS_LOG_TAG, __VA_ARGS__)

#else

#include <cstdio>

#define TS_LOGD(fmt, ...) fprintf(stderr, "[Typesetting D] " fmt "\n", ##__VA_ARGS__)
#define TS_LOGI(fmt, ...) fprintf(stderr, "[Typesetting I] " fmt "\n", ##__VA_ARGS__)
#define TS_LOGW(fmt, ...) fprintf(stderr, "[Typesetting W] " fmt "\n", ##__VA_ARGS__)

#endif
