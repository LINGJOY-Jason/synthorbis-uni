#pragma once

/**
 * @file compiler.h
 * @brief SynthOrbis UNI — 编译器特性检测
 *
 * 基于 macros.h，定义更高级的编译器行为宏。
 * 供 engine/ 和 platform/src/ 使用。
 */

// ─────────────────────────────────────────────────────────────
//  链接期函数注册（GCC / Clang）
// ─────────────────────────────────────────────────────────────

#if defined(SANCTIFY_COMPILER_GCC) || defined(SANCTIFY_COMPILER_CLANG)
  #define SANCTIFY_CTOR(fn) \
    __attribute__((constructor(101))) static void fn##_ctor(void)
  #define SANCTIFY_DTOR(fn) \
    __attribute__((destructor(101)))  static void fn##_dtor(void)
#elif defined(SANCTIFY_COMPILER_MSVC)
  #pragma section(".CRT$XCU", read)
  #define SANCTIFY_CTOR(fn) \
    static void __cdecl fn##_ctor(void); \
    __declspec(allocate(".CRT$XCU")) static void (__cdecl *fn##_ctor_ptr)(void) = fn##_ctor; \
    static void __cdecl fn##_ctor(void)
  #define SANCTIFY_DTOR(fn) \
    static void __cdecl fn##_dtor(void); \
    __declspec(allocate(".CRT$XTU")) static void (__cdecl *fn##_dtor_ptr)(void) = fn##_dtor; \
    static void __cdecl fn##_dtor(void)
#else
  #define SANCTIFY_CTOR(fn) static void fn(void)
  #define SANCTIFY_DTOR(fn) static void fn(void)
#endif

// ─────────────────────────────────────────────────────────────
//  Fallthrough 标记（C++17 / C11）
// ─────────────────────────────────────────────────────────────

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define SANCTIFY_FALLTHROUGH [[fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
  #define SANCTIFY_FALLTHROUGH __attribute__((fallthrough))
#else
  #define SANCTIFY_FALLTHROUGH ((void)0)
#endif

// ─────────────────────────────────────────────────────────────
//  协程支持检测
// ─────────────────────────────────────────────────────────────

#if defined(__cpp_coroutines) || \
    (defined(_MSC_VER) && _MSC_VER >= 1900) || \
    defined(__clang_major__) && __clang_major__ >= 16
  #define SANCTIFY_HAVE_COROUTINES 1
#else
  #define SANCTIFY_HAVE_COROUTINES 0
#endif

// ─────────────────────────────────────────────────────────────
//  泛型支持（C++20 / C11）
// ─────────────────────────────────────────────────────────────

#if defined(SANCTIFY_CPP20) || defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 202311L
  #define SANCTIFY_HAVE_GENERIC 1
#else
  #define SANCTIFY_HAVE_GENERIC 0
#endif

// ─────────────────────────────────────────────────────────────
//  模块支持
// ─────────────────────────────────────────────────────────────

#if defined(__cpp_modules) || defined(__MODULE__)
  #define SANCTIFY_HAVE_MODULES 1
#else
  #define SANCTIFY_HAVE_MODULES 0
#endif

// ─────────────────────────────────────────────────────────────
//  printf / scanf 格式检查
// ─────────────────────────────────────────────────────────────

#if defined(SANCTIFY_COMPILER_GCC) || defined(SANCTIFY_COMPILER_CLANG)
  #define SANCTIFY_PRINTF(fmt_idx, first_arg) \
    __attribute__((format(printf, fmt_idx, first_arg)))
  #define SANCTIFY_SCANF(fmt_idx, first_arg) \
    __attribute__((format(scanf, fmt_idx, first_arg)))
#elif defined(SANCTIFY_COMPILER_MSVC)
  #define SANCTIFY_PRINTF(fmt_idx, first_arg)
  #define SANCTIFY_SCANF(fmt_idx, first_arg)
#else
  #define SANCTIFY_PRINTF(fmt_idx, first_arg)
  #define SANCTIFY_SCANF(fmt_idx, first_arg)
#endif

// ─────────────────────────────────────────────────────────────
//  结构体大小检查（编译期安全）
// ─────────────────────────────────────────────────────────────

#define SANCTIFY_SIZEOF_MEMBER(type, member) \
  sizeof(((type*)0)->member)

#define SANCTIFY_OFFSETOF(type, member) \
  __builtin_offsetof(type, member)
