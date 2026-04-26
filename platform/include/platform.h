#pragma once

/**
 * @file platform.h
 * @brief SynthOrbis UNI - 跨平台抽象层核心头文件
 *
 * 所有平台相关代码必须包含此头文件。
 * 通过统一接口屏蔽 Windows/macOS/Linux/鸿蒙 的差异。
 *
 * @version 1.0.0
 * @date 2026-04-26
 */

#include "platform/macros.h"
#include "platform/types.h"
#include "platform/compiler.h"

// panic.h 需要放在 extern "C" 之外，因为包含 C++ 代码
#include "platform/panic.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  平台检测宏（优先级：专用 > 通用 Linux）
// ─────────────────────────────────────────────────────────────

#if defined(__SANCTIFY_X86__) || defined(SANCTIFY_WINDOWS)
  #define SANCTIFY_PLATFORM_WINDOWS 1
  #define SANCTIFY_PLATFORM_NAME    "Windows"
  #define SANCTIFY_PLATFORM_ICON     L"⊞"
#elif defined(__SANCTIFY_MACOS__) || defined(SANCTIFY_MACOS) || defined(__APPLE__)
  #define SANCTIFY_PLATFORM_MACOS   1
  #define SANCTIFY_PLATFORM_NAME    "macOS"
  #define SANCTIFY_PLATFORM_ICON     L"🍎"
#elif defined(__SANCTIFY_HARMONY__) || defined(SANCTIFY_HARMONYOS) || defined(__OHOS__)
  #define SANCTIFY_PLATFORM_HARMONY 1
  #define SANCTIFY_PLATFORM_NAME    "HarmonyOS"
  #define SANCTIFY_PLATFORM_ICON     L"𓊆"
#elif defined(__SANCTIFY_LINUX__) || defined(SANCTIFY_LINUX) || defined(__linux__)
  #define SANCTIFY_PLATFORM_LINUX   1

  // 进一步区分信创 Linux 发行版
  #if defined(__loongarch__)
    #define SANCTIFY_PLATFORM_LOONGARCH 1
    #define SANCTIFY_PLATFORM_NAME      "Linux (LoongArch)"
    #define SANCTIFY_PLATFORM_ICON       L"🐉"
  #elif defined(__aarch64__)
    #define SANCTIFY_PLATFORM_ARMV8  1
    #define SANCTIFY_PLATFORM_NAME   "Linux (ARM64)"
    #define SANCTIFY_PLATFORM_ICON   L"🪖"
  #else
    #define SANCTIFY_PLATFORM_NAME   "Linux"
    #define SANCTIFY_PLATFORM_ICON   L"🐧"
  #endif
#else
  #define SANCTIFY_PLATFORM_UNKNOWN 1
  #define SANCTIFY_PLATFORM_NAME    "Unknown"
  #define SANCTIFY_PLATFORM_ICON    L"?"
#endif

// ─────────────────────────────────────────────────────────────
//  CPU 架构检测
// ─────────────────────────────────────────────────────────────

#if defined(_M_X64) || defined(__x86_64__)
  #define SANCTIFY_ARCH_X86_64  1
  #define SANCTIFY_ARCH_NAME    "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
  #define SANCTIFY_ARCH_X86     1
  #define SANCTIFY_ARCH_NAME    "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
  #define SANCTIFY_ARCH_ARM64   1
  #define SANCTIFY_ARCH_NAME    "ARM64"
#elif defined(__arm__)
  #define SANCTIFY_ARCH_ARM     1
  #define SANCTIFY_ARCH_NAME    "ARM"
#elif defined(__loongarch64)
  #define SANCTIFY_ARCH_LOONG64 1
  #define SANCTIFY_ARCH_NAME    "LoongArch64"
#else
  #define SANCTIFY_ARCH_UNKNOWN  1
  #define SANCTIFY_ARCH_NAME     "unknown"
#endif

// ─────────────────────────────────────────────────────────────
//  特性开关（可由 CMake -D 覆盖）
// ─────────────────────────────────────────────────────────────

#ifndef SANCTIFY_FEATURE_AI
  #define SANCTIFY_FEATURE_AI 1          // AI 引擎（默认开启）
#endif

#ifndef SANCTIFY_FEATURE_ASR
  #define SANCTIFY_FEATURE_ASR 1          // 语音输入（默认开启）
#endif

#ifndef SANCTIFY_FEATURE_CLOUD
  #define SANCTIFY_FEATURE_CLOUD 1        // 云端 AI（默认开启）
#endif

#ifndef SANCTIFY_FEATURE_LOCAL_MODEL
  #define SANCTIFY_FEATURE_LOCAL_MODEL 1  // 本地模型推理（默认开启）
#endif

#ifndef SANCTIFY_FEATURE_X11
  #define SANCTIFY_FEATURE_X11 0         // X11 支持（Linux 默认关）
#endif

#ifndef SANCTIFY_FEATURE_WAYLAND
  #define SANCTIFY_FEATURE_WAYLAND 0      // Wayland 支持（实验性）
#endif

#ifndef SANCTIFY_FEATURE_IME_KIT
  #define SANCTIFY_FEATURE_IME_KIT 0      // HarmonyOS IME Kit（车机端）
#endif

#ifndef SANCTIFY_DEBUG
  #ifdef _DEBUG
    #define SANCTIFY_DEBUG 1
  #else
    #define SANCTIFY_DEBUG 0
  #endif
#endif

// ─────────────────────────────────────────────────────────────
//  版本信息
// ─────────────────────────────────────────────────────────────

#define SANCTIFY_VERSION_MAJOR 1
#define SANCTIFY_VERSION_MINOR 0
#define SANCTIFY_VERSION_PATCH 0

#define SANCTIFY_VERSION_STRING "1.0.0"
#define SANCTIFY_VERSION_COMPILEDATE __DATE__
#define SANCTIFY_VERSION_COMPILETIME __TIME__

// ─────────────────────────────────────────────────────────────
//  统一 C API（核心函数）
// ─────────────────────────────────────────────────────────────

/** 初始化平台抽象层（应在 main() 最早期调用） */
int sanctify_init(void);

/** 清理平台抽象层（应在 exit() 前调用） */
void sanctify_shutdown(void);

/** 获取当前平台名称（UTF-8） */
const char* sanctify_platform_name(void);

/** 获取 RIME 引擎版本 */
const char* sanctify_rime_version(void);

/** 获取构建时间戳 */
const char* sanctify_build_timestamp(void);

/** 检查是否为信创平台（龙芯/飞腾/鲲鹏/兆芯/麒麟/统信） */
int sanctify_is_xinchuang(void);

/** 获取可执行文件所在目录（UTF-8，调用者需 free） */
char* sanctify_get_exe_dir(void);

/** 获取用户配置目录（UTF-8，调用者需 free） */
char* sanctify_get_config_dir(void);

/** 获取日志文件路径（UTF-8，调用者需 free） */
char* sanctify_get_log_path(const char* module_name);

/** 获取临时文件目录（UTF-8，调用者需 free） */
char* sanctify_get_temp_dir(void);

/** 线程安全日志输出 */
void sanctify_log(int level, const char* module, const char* fmt, ...);

/** 获取系统 CPU 核心数 */
int sanctify_cpu_count(void);

/** 获取系统总内存（字节） */
uint64_t sanctify_total_memory(void);

#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────────────────────
//  C++ 命名空间封装
// ─────────────────────────────────────────────────────────────

#ifdef __cplusplus

namespace sanctify {

// 日志级别
enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO  = 1,
  LOG_WARN  = 2,
  LOG_ERROR = 3,
  LOG_FATAL = 4
};

// 平台信息结构体
struct PlatformInfo {
  const char* name;      // 平台名称
  const char* arch;      // CPU 架构
  bool        is_debug;  // 是否调试构建
  bool        is_xc;     // 是否信创平台
};

// 获取平台信息
inline PlatformInfo get_platform_info() {
  return {
    SANCTIFY_PLATFORM_NAME,
    SANCTIFY_ARCH_NAME,
    SANCTIFY_DEBUG != 0,
#if defined(SANCTIFY_PLATFORM_LOONGARCH) || \
    defined(SANCTIFY_PLATFORM_ARMV8) || \
    defined(__MIPS_XC__) || defined(__ZX_XC__)
    true
#else
    false
#endif
  };
}

// RAII 初始化封装
class InitGuard {
public:
  InitGuard() { sanctify_init(); }
  ~InitGuard() { sanctify_shutdown(); }
  InitGuard(const InitGuard&) = delete;
  InitGuard& operator=(const InitGuard&) = delete;
};

}  // namespace sanctify

#endif  // __cplusplus
