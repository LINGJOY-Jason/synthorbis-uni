#pragma once

/**
 * @file types.h
 * @brief SynthOrbis UNI — 统一类型定义
 *
 * 为所有平台提供一致的基础类型，避免 stdint.h / windows.h 冲突。
 * 所有 engine/ 和 platform/ 目录下的代码必须包含此文件。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

// ─────────────────────────────────────────────────────────────
//  跨平台固定宽度整数（优先使用 stdint.h）
// ─────────────────────────────────────────────────────────────

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;

// ─────────────────────────────────────────────────────────────
//  有符号指针宽度整数（用于数组索引、内存大小）
// ─────────────────────────────────────────────────────────────

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || \
    defined(__loongarch64) || defined(__LP64__)
  typedef int64_t  isize;
  typedef uint64_t usize;
#else
  typedef int32_t  isize;
  typedef uint32_t usize;
#endif

// ─────────────────────────────────────────────────────────────
//  字符类型（统一 UTF-8 / UTF-16 / WCHAR）
// ─────────────────────────────────────────────────────────────

typedef char     c8;      // UTF-8 字符（1~4 字节）
typedef uint16_t c16;     // UTF-16 字符（Windows wchar_t）
typedef uint32_t c32;     // UTF-32 字符

// ─────────────────────────────────────────────────────────────
//  跨平台 wchar_t 统一化
// ─────────────────────────────────────────────────────────────

#if defined(_WIN32)
  typedef wchar_t wc;
  #define SANCTIFY_WCHAR_SIZE 2
#else
  typedef wchar_t wc;
  #define SANCTIFY_WCHAR_SIZE 4
#endif

// ─────────────────────────────────────────────────────────────
//  跨平台路径分隔符
// ─────────────────────────────────────────────────────────────

#ifdef _WIN32
  #define SANCTIFY_PATH_SEP '\\'
  #define SANCTIFY_PATH_SEP_STR "\\"
#else
  #define SANCTIFY_PATH_SEP '/'
  #define SANCTIFY_PATH_SEP_STR "/"
#endif

// ─────────────────────────────────────────────────────────────
//  跨平台 API 导出宏
// ─────────────────────────────────────────────────────────────

#ifdef _WIN32
  #ifdef SANCTIFY_EXPORTS
    #define SANCTIFY_API __declspec(dllexport)
  #else
    #define SANCTIFY_API __declspec(dllimport)
  #endif
#else
  #define SANCTIFY_API __attribute__((visibility("default")))
#endif

// ─────────────────────────────────────────────────────────────
//  结果状态码（统一错误处理）
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyStatus {
  SANCTIFY_OK              =  0,
  SANCTIFY_ERROR           = -1,
  SANCTIFY_ERROR_NULLPTR  = -2,
  SANCTIFY_ERROR_MEMORY    = -3,
  SANCTIFY_ERROR_IO        = -4,
  SANCTIFY_ERROR_PLATFORM  = -5,
  SANCTIFY_ERROR_RIME      = -6,
  SANCTIFY_ERROR_AI        = -7,
  SANCTIFY_ERROR_ASR       = -8,
  SANCTIFY_ERROR_TIMEOUT   = -9,
  SANCTIFY_ERROR_CANCELLED = -10,
} SanctifyStatus;

// 状态码转字符串
static inline const char* sanctify_status_str(SanctifyStatus s) {
  switch (s) {
    case SANCTIFY_OK:              return "OK";
    case SANCTIFY_ERROR:           return "Unknown error";
    case SANCTIFY_ERROR_NULLPTR:   return "Null pointer";
    case SANCTIFY_ERROR_MEMORY:    return "Out of memory";
    case SANCTIFY_ERROR_IO:        return "I/O error";
    case SANCTIFY_ERROR_PLATFORM:   return "Platform error";
    case SANCTIFY_ERROR_RIME:       return "RIME engine error";
    case SANCTIFY_ERROR_AI:        return "AI engine error";
    case SANCTIFY_ERROR_ASR:       return "ASR error";
    case SANCTIFY_ERROR_TIMEOUT:   return "Operation timeout";
    case SANCTIFY_ERROR_CANCELLED: return "Operation cancelled";
    default:                        return "Unknown status";
  }
}

// ─────────────────────────────────────────────────────────────
//  字符串视图（零拷贝）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyStrView {
  const char* data;
  size_t      len;
} SanctifyStrView;

#define SANCTIFY_SV_LITERAL(str) \
  { str, sizeof(str) - 1 }

static inline bool sanctify_sv_eq(SanctifyStrView a, SanctifyStrView b) {
  return a.len == b.len && (a.len == 0 || \
    __builtin_memcmp(a.data, b.data, a.len) == 0);
}

// ─────────────────────────────────────────────────────────────
//  内存分配函数指针（支持自定义 allocator）
// ─────────────────────────────────────────────────────────────

typedef void* (*SanctifyAllocFn)(size_t size);
typedef void* (*SanctifyReallocFn)(void* ptr, size_t new_size);
typedef void  (*SanctifyFreeFn)(void* ptr);

typedef struct SanctifyAllocator {
  SanctifyAllocFn    alloc;
  SanctifyReallocFn   realloc;
  SanctifyFreeFn     free_;
} SanctifyAllocator;

extern SANCTIFY_API SanctifyAllocator sanctify_default_allocator;

// ─────────────────────────────────────────────────────────────
//  回调函数类型
// ─────────────────────────────────────────────────────────────

typedef void (*SanctifyLogCallback)(int level, const char* module,
                                    const char* message, void* userdata);

typedef void (*SanctifyProgressCallback)(float progress, void* userdata);

typedef void (*SanctifyAsyncCallback)(SanctifyStatus status, void* userdata);
