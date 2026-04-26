#pragma once

/**
 * @file context.h
 * @brief SynthOrbis UNI — 输入法上下文统一接口
 *
 * 抽象不同平台的输入法上下文（InputContext / InputMethodContext），
 * 为 AI 引擎提供统一的输入/候选/候选页访问接口。
 *
 * 平台实现：
 *  - Windows:   ImmGetContext / ImeToAsciiEx
 *  - macOS:     TSM / NSTextInputClient
 *  - Linux:     ibus / fcitx 内存结构
 *  - HarmonyOS: ArkTS InputMethodExtensionAbility
 */

#include "platform/types.h"
#include "platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  候选词
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyCandidate {
  char text[256];        // 候选词文本（UTF-8）
  char comment[256];     // 候选词注释（可选）
  char code[64];         // 对应编码（简拼/全拼等）
  int  weight;           // 候选词权重（用于 AI 重排序）
  bool is_ai_suggested;  // 是否为 AI 生成候选
} SanctifyCandidate;

// ─────────────────────────────────────────────────────────────
//  输入上下文状态
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyInputState {
  SANCTIFY_STATE_IDLE        = 0,  // 空闲（无输入）
  SANCTIFY_STATE_COMPOSING   = 1,  // 输入中（编码中）
  SANCTIFY_STATE_SELECTING   = 2,  // 选词中（候选列表打开）
  SANCTIFY_STATE_ENDING      = 3,  // 提交中（按下回车/空格）
} SanctifyInputState;

// ─────────────────────────────────────────────────────────────
//  编辑器操作命令
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyEditorCommand {
  SANCTIFY_CMD_COMMIT      = 0,  // 提交当前输入
  SANCTIFY_CMD_BACKSPACE   = 1,  // 删除一个字符
  SANCTIFY_CMD_ESCAPE      = 2,  // 取消输入
  SANCTIFY_CMD_ARROW_LEFT  = 3,  // 光标左移
  SANCTIFY_CMD_ARROW_RIGHT = 4,  // 光标右移
  SANCTIFY_CMD_HOME        = 5,  // 光标移到开头
  SANCTIFY_CMD_END         = 6,  // 光标移到末尾
  SANCTIFY_CMD_SELECT_PAGE_UP   = 7,  // 候选页上翻
  SANCTIFY_CMD_SELECT_PAGE_DOWN = 8,  // 候选页下翻
  SANCTIFY_CMD_SPACE       = 9,  // 空格（选第一候选）
  SANCTIFY_CMD_ENTER       = 10, // 回车（手工造词/确认）
  SANCTIFY_CMD_TAB         = 11, // Tab（切换到下一候选）
} SanctifyEditorCommand;

// ─────────────────────────────────────────────────────────────
//  输入法上下文接口（平台无关）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyInputContext SanctifyInputContext;

typedef struct SanctifyInputContextVTable {
  // 状态查询
  SanctifyInputState (*get_state)(SanctifyInputContext*);
  const char* (*get_preedit)(SanctifyInputContext*);  // 当前编码（UTF-8）
  int         (*get_caret_pos)(SanctifyInputContext*); // 光标位置

  // 候选词
  int  (*get_candidate_count)(SanctifyInputContext*);
  void (*get_candidates)(SanctifyInputContext*, SanctifyCandidate* out, int max);

  // 操作
  void (*select_candidate)(SanctifyInputContext*, int index); // 选第 N 个候选
  void (*highlight_candidate)(SanctifyInputContext*, int index); // 高亮（预览）
  void (*commit_text)(SanctifyInputContext*, const char* text); // 提交文本
  void (*delete_range)(SanctifyInputContext*, int start, int end); // 删除范围

  // 候选页
  void (*set_page_size)(SanctifyInputContext*, int page_size);
  void (*set_page_no)(SanctifyInputContext*, int page_no);
  int  (*get_page_size)(SanctifyInputContext*);
  int  (*get_page_count)(SanctifyInputContext*);
  int  (*get_page_no)(SanctifyInputContext*);

  // 生命周期
  void (*destroy)(SanctifyInputContext*);
} SanctifyInputContextVTable;

struct SanctifyInputContext {
  const SanctifyInputContextVTable* vtable;
  void* platform_handle;  // 平台原生句柄
  void* userdata;
};

// ─────────────────────────────────────────────────────────────
//  AI 候选词重排序回调
// ─────────────────────────────────────────────────────────────

/**
 * AI 重排序候选词（可由 AI 引擎调用）
 * @param ctx     输入上下文
 * @param in      原始候选列表
 * @param out     排序后的候选列表（可重用 in 缓冲区）
 * @param count   候选词数量
 * @param userdata  用户数据
 */
typedef void (*SanctifyAIReorderCallback)(
    SanctifyInputContext* ctx,
    SanctifyCandidate* in,
    SanctifyCandidate* out,
    int count,
    void* userdata
);

// ─────────────────────────────────────────────────────────────
//  RIME 输入引擎统一接口
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyRimeContext SanctifyRimeContext;

typedef struct SanctifyRimeVTable {
  /** 获取当前编码 */
  const char* (*get_preedit)(SanctifyRimeContext*);

  /** 获取光标位置 */
  int         (*get_caret_pos)(SanctifyRimeContext*);

  /** 获取候选词列表 */
  int         (*get_candidates)(SanctifyRimeContext*,
                                SanctifyCandidate* out, int max);

  /** 模拟按键（核心处理函数） */
  bool        (*process_key)(SanctifyRimeContext*, int keycode, int mask);

  /** 提交上屏 */
  void        (*commit)(SanctifyRimeContext*);

  /** 清除输入 */
  void        (*clear)(SanctifyRimeContext*);

  /** 切换输入法 */
  void        (*select_schema)(SanctifyRimeContext*, const char* schema_id);

  /** 获取当前方案名 */
  const char* (*get_current_schema)(SanctifyRimeContext*);

  void         (*destroy)(SanctifyRimeContext*);
} SanctifyRimeVTable;

struct SanctifyRimeContext {
  const SanctifyRimeVTable* vtable;
  void* rime_ptr;       // librime::Engine*
  void* userdata;
};

// ─────────────────────────────────────────────────────────────
//  统一工厂函数
// ─────────────────────────────────────────────────────────────

/** 创建平台输入法上下文 */
SANCTIFY_API SanctifyStatus
sanctify_context_create(void* platform_window, SanctifyInputContext** out);

/** 创建 RIME 引擎实例 */
SANCTIFY_API SanctifyStatus
sanctify_rime_create(const char* shared_dir,
                      const char* user_dir,
                      SanctifyRimeContext** out);

/** 获取 RIME 版本 */
SANCTIFY_API const char*
sanctify_rime_get_version(void);

/** 加载 RIME 方案列表 */
SANCTIFY_API SanctifyStatus
sanctify_rime_list_schemas(SanctifyRimeContext*,
                           char(*out_ids)[64], int max, int* out_count);

/** 设置 AI 候选重排序回调 */
SANCTIFY_API void
sanctify_rime_set_ai_callback(SanctifyRimeContext*,
                               SanctifyAIReorderCallback cb,
                               void* userdata);

// ─────────────────────────────────────────────────────────────
//  C++ RAII 封装
// ─────────────────────────────────────────────────────────────

#ifdef __cplusplus

namespace sanctify {

// 候选词列表（RAII 封装）
class CandidateList {
public:
  CandidateList() : count_(0), candidates_(nullptr) {}
  explicit CandidateList(SanctifyRimeContext* ctx) {
    SanctifyCandidate buf[32];
    count_ = ctx->vtable->get_candidates(ctx, buf, 32);
    if (count_ > 0) {
      data_.resize(count_);
      memcpy(data_.data(), buf, count_ * sizeof(SanctifyCandidate));
      candidates_ = data_.data();
    }
  }

  int size() const { return count_; }
  bool empty() const { return count_ == 0; }

  SanctifyCandidate& operator[](int i) { return candidates_[i]; }
  const SanctifyCandidate& operator[](int i) const { return candidates_[i]; }

  int page_count(int page_size) const {
    return (count_ + page_size - 1) / page_size;
  }

  int page_start(int page_no, int page_size) const {
    return page_no * page_size;
  }

  int page_end(int page_no, int page_size) const {
    int s = page_no * page_size;
    int e = s + page_size;
    return e < count_ ? e : count_;
  }

  SanctifyCandidate* data() { return candidates_; }

private:
  int count_;
  SanctifyCandidate* candidates_;
  std::vector<SanctifyCandidate> data_;
};

}  // namespace sanctify

#endif  // __cplusplus
