#pragma once

/**
 * @file panic.h
 * @brief SynthOrbis UNI - 跨平台异常与终止处理
 *
 * 提供统一的 fatal error 处理机制，
 * 在所有平台上保持一致的 abort 行为。
 */

#include "platform/types.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
#include <exception>
#endif

// ─────────────────────────────────────────────────────────────
//  Panic 级别
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyPanicLevel {
    SANCTIFY_PANIC_ASSERT,  // 断言失败
    SANCTIFY_PANIC_OOM,     // 内存耗尽
    SANCTIFY_PANIC_FATAL,   // 不可恢复的致命错误
    SANCTIFY_PANIC_RUNTIME  // 运行时错误
} SanctifyPanicLevel;

// ─────────────────────────────────────────────────────────────
//  Panic 回调
// ─────────────────────────────────────────────────────────────

typedef void (*SanctifyPanicCallback)(
    SanctifyPanicLevel level,
    const char* file,
    int line,
    const char* func,
    const char* msg
);

/**
 * 设置自定义 panic 回调
 */
SANCTIFY_API void sanctify_set_panic_callback(SanctifyPanicCallback cb);

/**
 * 触发 fatal panic（永不返回）
 */
SANCTIFY_API void
sanctify_panic(SanctifyPanicLevel level,
               const char* file,
               int line,
               const char* func,
               const char* fmt,
               ...);

// ─────────────────────────────────────────────────────────────
//  便捷宏
// ─────────────────────────────────────────────────────────────

/** 断言失败时 panic */
#define SANCTIFY_PANIC(msg) \
    sanctify_panic(SANCTIFY_PANIC_ASSERT, __FILE__, __LINE__, \
                   __func__, "%s", msg)

/** 内存分配失败时 panic */
#define SANCTIFY_PANIC_OOM() \
    sanctify_panic(SANCTIFY_PANIC_OOM, __FILE__, __LINE__, \
                   __func__, "Out of memory")

/** 致命错误 panic */
#define SANCTIFY_FATAL(msg) \
    sanctify_panic(SANCTIFY_PANIC_FATAL, __FILE__, __LINE__, \
                   __func__, "%s", msg)

/** 带格式化消息的 panic */
#define SANCTIFY_PANICF(...) \
    sanctify_panic(SANCTIFY_PANIC_RUNTIME, __FILE__, __LINE__, \
                   __func__, __VA_ARGS__)

// ─────────────────────────────────────────────────────────────
//  Signal 捕获（仅 POSIX）
// ─────────────────────────────────────────────────────────────

#if !defined(_WIN32)
    #include <signal.h>
    #include <setjmp.h>

    SANCTIFY_API void sanctify_install_signal_handlers(void);
    SANCTIFY_API void sanctify_remove_signal_handlers(void);

    typedef void (*SanctifySigHandler)(int sig, void* ctx);
    SANCTIFY_API void sanctify_on_signal(int sig, SanctifySigHandler h, void* ud);
#endif

// ─────────────────────────────────────────────────────────────
//  C++ 异常安全封装
// ─────────────────────────────────────────────────────────────

#ifdef __cplusplus
namespace sanctify {

// RAII scope guard
class ScopeGuard {
public:
    explicit ScopeGuard(void(*fn)(void)) : fn_(fn) {}
    ~ScopeGuard() { if (fn_) fn_(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    void dismiss() { fn_ = nullptr; }
private:
    void(*fn_)(void);
};

// RAII 唯一指针
template <typename T, void(*Del)(T*)>
class UniquePtr {
public:
    explicit UniquePtr(T* p = nullptr) : p_(p) {}
    ~UniquePtr() { if (p_) Del(p_); }
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;
    T* get() const { return p_; }
    T* release() { T* r = p_; p_ = nullptr; return r; }
    void reset(T* p) { if (p_) Del(p_); p_ = p; }
    explicit operator bool() const { return p_ != nullptr; }
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }
private:
    T* p_;
};

}  // namespace sanctify

#define SANCTIFY_SCOPE_GUARD(fn) \
    sanctify::ScopeGuard SANCTIFY_CONCAT(_sg_, __LINE__)(fn)

#endif  // __cplusplus
