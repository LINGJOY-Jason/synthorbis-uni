#pragma once

/**
 * @file macros.h
 * @brief SynthOrbis UNI — 编译器与平台宏
 */

// ─────────────────────────────────────────────────────────────
//  编译器检测
// ─────────────────────────────────────────────────────────────

#if defined(__clang__)
  #define SANCTIFY_COMPILER_CLANG  1
  #define SANCTIFY_COMPILER_NAME   "Clang"
  #define SANCTIFY_COMPILER_VER    __clang_version__
#elif defined(__GNUC__)
  #define SANCTIFY_COMPILER_GCC    1
  #define SANCTIFY_COMPILER_NAME   "GCC"
  #define SANCTIFY_COMPILER_VER    __VERSION__
#elif defined(_MSC_VER)
  #define SANCTIFY_COMPILER_MSVC   1
  #define SANCTIFY_COMPILER_NAME   "MSVC"
  #define SANCTIFY_COMPILER_VER    _MSC_FULL_VER
#elif defined(__clang_analyzer__)
  #define SANCTIFY_COMPILER_ANALYZER 1
#else
  #define SANCTIFY_COMPILER_UNKNOWN 1
  #define SANCTIFY_COMPILER_NAME     "Unknown"
#endif

// ─────────────────────────────────────────────────────────────
//  C++ 标准
// ─────────────────────────────────────────────────────────────

#if defined(__cplusplus)
  #if __cplusplus >= 202302L
    #define SANCTIFY_CPP23 1
  #elif __cplusplus >= 202002L
    #define SANCTIFY_CPP20 1
  #elif __cplusplus >= 201703L
    #define SANCTIFY_CPP17 1
  #elif __cplusplus >= 201402L
    #define SANCTIFY_CPP14 1
  #else
    #define SANCTIFY_CPP11 1
  #endif
#endif

// ─────────────────────────────────────────────────────────────
//  跨平台警告抑制与属性
// ─────────────────────────────────────────────────────────────

// 警告抑制（局部使用）
#if defined(SANCTIFY_COMPILER_CLANG) || defined(SANCTIFY_COMPILER_GCC)
  #define SANCTIFY_WARN_UNUSED         __attribute__((warn_unused_result))
  #define SANCTIFY_WARN_DISABLE(w)     _Pragma("GCC diagnostic push") \
                                        _Pragma(GCC diagnostic ignored #w)
  #define SANCTIFY_WARN_RESTORE         _Pragma("GCC diagnostic pop")
  #define SANCTIFY_NOINLINE             __attribute__((noinline))
  #define SANCTIFY_INLINE               inline __attribute__((always_inline))
  #define SANCTIFY_HIDDEN               __attribute__((visibility("hidden")))
  #define SANCTIFY_LIKELY(x)            __builtin_expect(!!(x), 1)
  #define SANCTIFY_UNLIKELY(x)          __builtin_expect(!!(x), 0)
#elif defined(SANCTIFY_COMPILER_MSVC)
  #define SANCTIFY_WARN_UNUSED
  #define SANCTIFY_NOINLINE             __declspec(noinline)
  #define SANCTIFY_INLINE               __forceinline
  #define SANCTIFY_HIDDEN
  #define SANCTIFY_LIKELY(x)   (!!(x))
  #define SANCTIFY_UNLIKELY(x) (!!(x))
  #define __builtin_offsetof(type, member) __offsetof(type, member)
#else
  #define SANCTIFY_WARN_UNUSED
  #define SANCTIFY_NOINLINE
  #define SANCTIFY_INLINE        inline
  #define SANCTIFY_HIDDEN
  #define SANCTIFY_LIKELY(x)     (!!(x))
  #define SANCTIFY_UNLIKELY(x)    (!!(x))
#endif

// 废弃 API 标记
#if defined(SANCTIFY_COMPILER_CLANG) || defined(SANCTIFY_COMPILER_GCC)
  #define SANCTIFY_DEPRECATED(msg)  __attribute__((deprecated(msg)))
#elif defined(SANCTIFY_COMPILER_MSVC)
  #define SANCTIFY_DEPRECATED(msg)  __declspec(deprecated(msg))
#else
  #define SANCTIFY_DEPRECATED(msg)
#endif

// 跨平台 alignas / alignof 兼容
#if defined(SANCTIFY_COMPILER_MSVC) && _MSC_VER < 1900
  #define alignas(n)      __declspec(align(n))
  #define alignof(type)   __alignof(type)
#endif

// ─────────────────────────────────────────────────────────────
//  跨平台字节序检测
// ─────────────────────────────────────────────────────────────

#if defined(__BYTE_ORDER__)
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SANCTIFY_BYTEORDER_LITTLE 1
  #else
    #define SANCTIFY_BYTEORDER_BIG    1
  #endif
#elif defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || \
      defined(_M_ARM64) || defined(__i386__) || defined(__x86_64__) || \
      defined(__aarch64__) || defined(__loongarch__)
  #define SANCTIFY_BYTEORDER_LITTLE 1
#else
  #define SANCTIFY_BYTEORDER_BIG  1
#endif

// ─────────────────────────────────────────────────────────────
//  跨平台 dllexport / calling convention
// ─────────────────────────────────────────────────────────────

#ifdef _WIN32
  #define SANCTIFY_CALL __cdecl
  #if !defined(SANCTIFY_DLLEXPORT)
    #define SANCTIFY_DLLEXPORT __declspec(dllexport)
  #endif
#else
  #define SANCTIFY_CALL
  #define SANCTIFY_DLLEXPORT __attribute__((visibility("default")))
#endif

// ─────────────────────────────────────────────────────────────
//  静态断言（C11 / C++11）
// ─────────────────────────────────────────────────────────────

#if defined(__cplusplus) && __cplusplus >= 201103L
  #define SANCTIFY_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  #define SANCTIFY_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#elif defined(SANCTIFY_COMPILER_MSVC)
  #define SANCTIFY_STATIC_ASSERT(expr, msg) \
    static_assert(sizeof(char[(expr) ? 1 : -1]) == 1, msg)
#else
  #define SANCTIFY_STATIC_ASSERT(expr, msg)
#endif

// ─────────────────────────────────────────────────────────────
//  内存屏障与原子操作（跨平台）
// ─────────────────────────────────────────────────────────────

#if defined(_M_ARM) || defined(_M_ARM64) || defined(__ARM_ARCH)
  #define SANCTIFY_MEMORY_BARRIER() __asm__ __volatile__("dmb sy" ::: "memory")
#elif defined(__x86_64__) || defined(_M_X64)
  #define SANCTIFY_MEMORY_BARRIER() __asm__ __volatile__("" ::: "memory")
#elif defined(__loongarch__)
  #define SANCTIFY_MEMORY_BARRIER() __asm__ __volatile__("dbar 0" ::: "memory")
#else
  #define SANCTIFY_MEMORY_BARRIER()
#endif

// ─────────────────────────────────────────────────────────────
//  调试辅助
// ─────────────────────────────────────────────────────────────

#if SANCTIFY_DEBUG
  #include <stdio.h>
  #define SANCTIFY_ASSERT(expr, msg) \
    do { \
      if (!(expr)) { \
        fprintf(stderr, "[SANCTIFY ASSERT] %s:%d: %s — %s\n", \
                __FILE__, __LINE__, #expr, msg); \
        *(volatile int*)0 = 0; \
      } \
    } while (0)
  #define SANCTIFY_TRACE(msg) \
    fprintf(stderr, "[TRACE] %s:%d: %s\n", __FILE__, __LINE__, msg)
#else
  #define SANCTIFY_ASSERT(expr, msg)     ((void)0)
  #define SANCTIFY_TRACE(msg)           ((void)0)
#endif

// ─────────────────────────────────────────────────────────────
//  宏技巧
// ─────────────────────────────────────────────────────────────

#define SANCTIFY_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SANCTIFY_CONCAT_IMPL(a, b) a##b
#define SANCTIFY_CONCAT(a, b)      SANCTIFY_CONCAT_IMPL(a, b)

#define SANCTIFY_STRINGIFY_IMPL(x) #x
#define SANCTIFY_STRINGIFY(x)       SANCTIFY_STRINGIFY_IMPL(x)

#define SANCTIFY_UNUSED(x) (void)(x)

// ─────────────────────────────────────────────────────────────
//  静态库导出（对外暴露给 librime 插件）
// ─────────────────────────────────────────────────────────────

#define SANCTIFY_PUBLIC_API SANCTIFY_API
