#pragma once

/**
 * @file thread.h
 * @brief SynthOrbis UNI — 跨平台线程与并发抽象
 *
 * 统一 Windows / POSIX / HarmonyOS 线程、互斥锁、条件变量、
 * 原子操作、异步任务接口。
 */

#include "platform/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  线程
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyThread SanctifyThread;

/**
 * 创建线程
 * @param fn       线程函数
 * @param arg      传递给函数的参数
 * @param name     线程名称（调试用，UTF-8）
 * @param stack_kb 栈大小（KB），0=系统默认
 */
SANCTIFY_API SanctifyThread*
sanctify_thread_create(void (*fn)(void*), void* arg,
                       const char* name, uint32_t stack_kb);

/** 启动线程 */
SANCTIFY_API SanctifyStatus
sanctify_thread_start(SanctifyThread*);

/** 等待线程结束 */
SANCTIFY_API SanctifyStatus
sanctify_thread_join(SanctifyThread*, uint32_t timeout_ms);

/** 获取当前线程 ID（平台无关 opaque 值） */
SANCTIFY_API uint64_t
sanctify_thread_current_id(void);

/** 设置线程名称 */
SANCTIFY_API void
sanctify_thread_set_name(SanctifyThread*, const char* name);

/** 让出 CPU 时间片 */
SANCTIFY_API void
sanctify_thread_yield(void);

/** 睡眠（毫秒） */
SANCTIFY_API void
sanctify_thread_sleep(uint32_t ms);

/** 销毁线程 */
SANCTIFY_API void
sanctify_thread_destroy(SanctifyThread*);

// ─────────────────────────────────────────────────────────────
//  互斥锁
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyMutex SanctifyMutex;
typedef struct SanctifyRWLock SanctifyRWLock;

SANCTIFY_API SanctifyMutex*  sanctify_mutex_create(void);
SANCTIFY_API void             sanctify_mutex_lock(SanctifyMutex*);
SANCTIFY_API bool             sanctify_mutex_try_lock(SanctifyMutex*);
SANCTIFY_API void             sanctify_mutex_unlock(SanctifyMutex*);
SANCTIFY_API void             sanctify_mutex_destroy(SanctifyMutex*);

// ─────────────────────────────────────────────────────────────
//  读写锁
// ─────────────────────────────────────────────────────────────

SANCTIFY_API SanctifyRWLock*  sanctify_rwlock_create(void);
SANCTIFY_API void             sanctify_rwlock_read_lock(SanctifyRWLock*);
SANCTIFY_API void             sanctify_rwlock_read_unlock(SanctifyRWLock*);
SANCTIFY_API void             sanctify_rwlock_write_lock(SanctifyRWLock*);
SANCTIFY_API void             sanctify_rwlock_write_unlock(SanctifyRWLock*);
SANCTIFY_API void             sanctify_rwlock_destroy(SanctifyRWLock*);

// ─────────────────────────────────────────────────────────────
//  条件变量
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyCond SanctifyCond;

SANCTIFY_API SanctifyCond*    sanctify_cond_create(void);
SANCTIFY_API void             sanctify_cond_wait(SanctifyCond*, SanctifyMutex*);
SANCTIFY_API bool             sanctify_cond_wait_timeout(SanctifyCond*,
                                                          SanctifyMutex*,
                                                          uint32_t ms);
SANCTIFY_API void             sanctify_cond_signal(SanctifyCond*);
SANCTIFY_API void             sanctify_cond_broadcast(SanctifyCond*);
SANCTIFY_API void             sanctify_cond_destroy(SanctifyCond*);

// ─────────────────────────────────────────────────────────────
//  原子操作（基础版）
// ─────────────────────────────────────────────────────────────

SANCTIFY_API int32_t  sanctify_atomic_load_i32(volatile int32_t*);
SANCTIFY_API void     sanctify_atomic_store_i32(volatile int32_t*, int32_t);
SANCTIFY_API int32_t  sanctify_atomic_fetch_add_i32(volatile int32_t*, int32_t);
SANCTIFY_API bool     sanctify_atomic_compare_exchange_i32(volatile int32_t*,
                                                            int32_t expected,
                                                            int32_t desired);

SANCTIFY_API int64_t  sanctify_atomic_load_i64(volatile int64_t*);
SANCTIFY_API void     sanctify_atomic_store_i64(volatile int64_t*, int64_t);
SANCTIFY_API int64_t  sanctify_atomic_fetch_add_i64(volatile int64_t*, int64_t);

SANCTIFY_API void*    sanctify_atomic_load_ptr(void* volatile*);
SANCTIFY_API void     sanctify_atomic_store_ptr(void* volatile*, void*);

// ─────────────────────────────────────────────────────────────
//  线程局部存储（TLS）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyTLS SanctifyTLS;

SANCTIFY_API SanctifyTLS*  sanctify_tls_create(void (*destroy_fn)(void*));
SANCTIFY_API void          sanctify_tls_set(SanctifyTLS*, void* value);
SANCTIFY_API void*         sanctify_tls_get(SanctifyTLS*);
SANCTIFY_API void          sanctify_tls_destroy(SanctifyTLS*);

// ─────────────────────────────────────────────────────────────
//  异步任务（Future / Promise 简化版）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyFuture SanctifyFuture;

typedef void (*SanctifyAsyncFn)(void* arg, void (*cb)(void*, SanctifyStatus), void*);
typedef void (*SanctifyFutureCallback)(SanctifyStatus, void*);

SANCTIFY_API SanctifyFuture*
sanctify_future_create(void);

SANCTIFY_API void
sanctify_future_resolve(SanctifyFuture*, void* result);

SANCTIFY_API void
sanctify_future_reject(SanctifyFuture*, SanctifyStatus status, const char* msg);

SANCTIFY_API SanctifyStatus
sanctify_future_await(SanctifyFuture*, uint32_t timeout_ms);

SANCTIFY_API void
sanctify_future_then(SanctifyFuture*, SanctifyFutureCallback cb, void* userdata);

SANCTIFY_API void*
sanctify_future_get_result(SanctifyFuture*);

SANCTIFY_API void
sanctify_future_destroy(SanctifyFuture*);

/** 在线程池中异步执行任务 */
SANCTIFY_API void
sanctify_async_run(void (*fn)(void*), void* arg);

// ─────────────────────────────────────────────────────────────
//  C++ RAII 封装
// ─────────────────────────────────────────────────────────────

#ifdef __cplusplus

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <future>
#include <condition_variable>

namespace sanctify {

// 互斥锁 RAII 封装
class LockGuard {
public:
  explicit LockGuard(SanctifyMutex* m) : m_(m) { sanctify_mutex_lock(m_); }
  ~LockGuard() { sanctify_mutex_unlock(m_); }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;
private:
  SanctifyMutex* m_;
};

// 读写锁 RAII 封装
class ReadLock {
public:
  explicit ReadLock(SanctifyRWLock* l) : l_(l) { sanctify_rwlock_read_lock(l_); }
  ~ReadLock() { sanctify_rwlock_read_unlock(l_); }
  ReadLock(const ReadLock&) = delete;
private:
  SanctifyRWLock* l_;
};

class WriteLock {
public:
  explicit WriteLock(SanctifyRWLock* l) : l_(l) { sanctify_rwlock_write_lock(l_); }
  ~WriteLock() { sanctify_rwlock_write_unlock(l_); }
  WriteLock(const WriteLock&) = delete;
private:
  SanctifyRWLock* l_;
};

// 跨平台线程池
class ThreadPool {
public:
  explicit ThreadPool(int threads = 0) {
    if (threads <= 0) threads = (int)sanctify_cpu_count();
    for (int i = 0; i < threads; ++i) {
      threads_.emplace_back([this] { worker_loop(); });
    }
  }

  ~ThreadPool() {
    shutdown_.store(true);
    cv_.notify_all();
    for (auto& t : threads_) t.join();
  }

  template <typename F>
  auto enqueue(F&& fn) -> std::future<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
    {
      std::lock_guard<std::mutex> l(queue_mutex_);
      if (tasks_.size() >= max_queue_size_) {
        return {};  // 队列满，返回空 future
      }
      tasks_.push([task]() { (*task)(); });
    }
    cv_.notify_one();
    return task->get_future();
  }

private:
  void worker_loop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> l(queue_mutex_);
        cv_.wait(l, [this] { return shutdown_.load() || !tasks_.empty(); });
        if (shutdown_ && tasks_.empty()) return;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable cv_;
  std::atomic<bool> shutdown_{false};
  size_t max_queue_size_ = 1024;
};

}  // namespace sanctify

#endif  // __cplusplus
