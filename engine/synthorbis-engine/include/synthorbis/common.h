// SynthOrbis Common — 通用定义
//
// 跨平台通用类型和宏定义

#ifndef SYNTHORBIS_COMMON_H_
#define SYNTHORBIS_COMMON_H_

// 平台检测
#if defined(_WIN32) || defined(_WIN64)
    #define SYNTHORBIS_PLATFORM_WINDOWS 1
    #if defined(_WIN64)
        #define SYNTHORBIS_ARCH_64BIT 1
    #else
        #define SYNTHORBIS_ARCH_32BIT 1
    #endif
#elif defined(__APPLE__) || defined(__MACH__)
    #define SYNTHORBIS_PLATFORM_MACOS 1
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define SYNTHORBIS_PLATFORM_MACOS 1
    #endif
#elif defined(__linux__)
    #define SYNTHORBIS_PLATFORM_LINUX 1
    #if defined(__aarch64__)
        #define SYNTHORBIS_ARCH_ARM64 1
    #elif defined(__arm__)
        #define SYNTHORBIS_ARCH_ARM 1
    #elif defined(__loongarch__)
        #define SYNTHORBIS_ARCH_LOONGARCH 1
    #else
        #define SYNTHORBIS_ARCH_X86 1
        #if defined(__x86_64__)
            #define SYNTHORBIS_ARCH_X86_64 1
        #endif
    #endif
#elif defined(__HarmonyOS__) || defined(__ohos__)
    #define SYNTHORBIS_PLATFORM_HARMONY 1
    #define SYNTHORBIS_PLATFORM_VEHICLE 1
#endif

// C++ 标准检测
#if defined(__cplusplus)
    #if __cplusplus >= 201703L
        #define SYNTHORBIS_CXX17 1
    #endif
    #if __cplusplus >= 202002L
        #define SYNTHORBIS_CXX20 1
    #endif
#endif

// 通用头文件
#include <stddef.h>
#include <stdint.h>

// 导出宏
#if defined(SYNTHORBIS_PLATFORM_WINDOWS)
    #ifdef SYNTHORBIS_EXPORTS
        #define SYNTHORBIS_API __declspec(dllexport)
    #else
        #define SYNTHORBIS_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define SYNTHORBIS_API __attribute__((visibility("default")))
#else
    #define SYNTHORBIS_API
#endif

// 简化宏
#ifdef __cplusplus
    #define SYNTHORBIS_BEGIN_DECLS extern "C" {
    #define SYNTHORBIS_END_DECLS }
#else
    #define SYNTHORBIS_BEGIN_DECLS
    #define SYNTHORBIS_END_DECLS
#endif

// 工具宏
#define SYNTHORBIS_VERSION_MAJOR 1
#define SYNTHORBIS_VERSION_MINOR 0
#define SYNTHORBIS_VERSION_PATCH 0

#define SYNTHORBIS_VERSION_STRING "1.0.0"

#endif  // SYNTHORBIS_COMMON_H_
