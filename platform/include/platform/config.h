#pragma once

/**
 * @file config.h
 * @brief SynthOrbis UNI — 统一配置存储抽象
 *
 * 统一配置文件读写（YAML / JSON / INI），
 * 适配 Windows 注册表 / macOS UserDefaults / Linux XDG
 * 以及 HarmonyOS preferences API。
 */

#include "platform/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  配置值类型
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyConfigValueType {
  SANCTIFY_CFG_NULL    = 0,
  SANCTIFY_CFG_BOOL    = 1,
  SANCTIFY_CFG_INT     = 2,
  SANCTIFY_CFG_DOUBLE  = 3,
  SANCTIFY_CFG_STRING  = 4,
  SANCTIFY_CFG_LIST    = 5,
  SANCTIFY_CFG_DICT    = 6,
} SanctifyConfigValueType;

typedef struct SanctifyConfigValue {
  SanctifyConfigValueType type;
  union {
    bool       bool_val;
    int64_t    int_val;
    double     double_val;
    char       str_val[512];
  };
} SanctifyConfigValue;

// ─────────────────────────────────────────────────────────────
//  配置节点（树形结构）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyConfigNode SanctifyConfigNode;

typedef struct SanctifyConfigIterator {
  SanctifyConfigNode* node;
  int   index;
} SanctifyConfigIterator;

typedef struct SanctifyConfig SanctifyConfig;

struct SanctifyConfig {
  void* impl;  // 平台实现数据

  // 读取
  SanctifyStatus (*get_bool)   (SanctifyConfig*, const char* key, bool* out);
  SanctifyStatus (*get_int)    (SanctifyConfig*, const char* key, int64_t* out);
  SanctifyStatus (*get_double)(SanctifyConfig*, const char* key, double* out);
  SanctifyStatus (*get_str)    (SanctifyConfig*, const char* key,
                                char* out, size_t max_len);

  // 写入
  SanctifyStatus (*set_bool)   (SanctifyConfig*, const char* key, bool val);
  SanctifyStatus (*set_int)    (SanctifyConfig*, const char* key, int64_t val);
  SanctifyStatus (*set_double) (SanctifyConfig*, const char* key, double val);
  SanctifyStatus (*set_str)    (SanctifyConfig*, const char* key, const char* val);

  // 层级操作
  SanctifyStatus (*get_section) (SanctifyConfig*, const char* path,
                                  SanctifyConfig* out_section);
  SanctifyStatus (*set_section) (SanctifyConfig*, const char* path);

  // 列表操作
  SanctifyStatus (*list_append) (SanctifyConfig*, const char* path,
                                  SanctifyConfigValue* val);
  int            (*list_size)    (SanctifyConfig*, const char* path);

  // 序列化
  SanctifyStatus (*save)        (SanctifyConfig*);
  SanctifyStatus (*load)        (SanctifyConfig*, const char* path);

  // 迭代
  SanctifyConfigIterator* (*iter_begin) (SanctifyConfig*, const char* path);
  SanctifyConfigIterator* (*iter_next)  (SanctifyConfigIterator*);
  const char*             (*iter_key)    (SanctifyConfigIterator*);
  SanctifyConfigValue*    (*iter_value) (SanctifyConfigIterator*);
  void                    (*iter_end)    (SanctifyConfigIterator*);

  void (*destroy)(SanctifyConfig*);
};

// ─────────────────────────────────────────────────────────────
//  统一配置管理器
// ─────────────────────────────────────────────────────────────

/** 打开/创建配置文件（YAML 格式，跨平台通用） */
SANCTIFY_API SanctifyStatus
sanctify_config_open(const char* path, bool create_if_missing,
                     SanctifyConfig** out_cfg);

/** 打开用户配置目录下的配置文件 */
SANCTIFY_API SanctifyStatus
sanctify_config_open_user(const char* filename,
                          bool create_if_missing,
                          SanctifyConfig** out_cfg);

/** 获取内置默认配置值（回退） */
SANCTIFY_API SanctifyStatus
sanctify_config_get_default(const char* key, SanctifyConfigValue* out);

/** 注册配置变更回调 */
typedef void (*SanctifyConfigChangedCallback)(const char* key, void* userdata);
SANCTIFY_API void
sanctify_config_on_changed(const char* key_pattern,
                           SanctifyConfigChangedCallback cb,
                           void* userdata);

/** 销毁配置对象 */
SANCTIFY_API void
sanctify_config_destroy(SanctifyConfig* cfg);

// ─────────────────────────────────────────────────────────────
//  预定义配置键（统一命名空间）
// ─────────────────────────────────────────────────────────────

// AI 配置
#define SANCTIFY_CFG_KEY_AI_ENABLED          "ai.enabled"
#define SANCTIFY_CFG_KEY_AI_MODEL_PATH      "ai.model_path"
#define SANCTIFY_CFG_KEY_AI_PROVIDER        "ai.provider"       // zhipu / doubao / local
#define SANCTIFY_CFG_KEY_AI_API_KEY         "ai.api_key"
#define SANCTIFY_CFG_KEY_AI_TEMPERATURE     "ai.temperature"
#define SANCTIFY_CFG_KEY_AI_MAX_TOKENS      "ai.max_tokens"

// ASR 配置
#define SANCTIFY_CFG_KEY_ASR_ENABLED        "asr.enabled"
#define SANCTIFY_CFG_KEY_ASR_MODEL          "asr.model"         // funasr / glm-asr
#define SANCTIFY_CFG_KEY_ASR_SAMPLE_RATE     "asr.sample_rate"
#define SANCTIFY_CFG_KEY_ASR_LANGUAGE       "asr.language"      // zh / en

// 界面配置
#define SANCTIFY_CFG_KEY_UI_CANDIDATE_COUNT  "ui.candidate_count"  // 每页候选数
#define SANCTIFY_CFG_KEY_UI_CANDIDATE_STYLE  "ui.candidate_style"  // vertical / horizontal
#define SANCTIFY_CFG_KEY_UI_FONT_FAMILY     "ui.font_family"
#define SANCTIFY_CFG_KEY_UI_FONT_SIZE        "ui.font_size"
#define SANCTIFY_CFG_KEY_UI_THEME            "ui.theme"           // dark / light
#define SANCTIFY_CFG_KEY_UI_BORDER_COLOR     "ui.border_color"
#define SANCTIFY_CFG_KEY_UI_ANIMATION        "ui.animation"
#define SANCTIFY_CFG_KEY_UI_CANDIDATE_COLOR  "ui.candidate_color"

// 音效配置
#define SANCTIFY_CFG_KEY_SOUND_ENABLED       "sound.enabled"
#define SANCTIFY_CFG_KEY_SOUND_VOLUME        "sound.volume"
#define SANCTIFY_CFG_KEY_SOUND_KEY_CLICK     "sound.key_click"
#define SANCTIFY_CFG_KEY_SOUND_ENGAGE        "sound.engage"

// 输入方案配置
#define SANCTIFY_CFG_KEY_SCHEMA_DEFAULT      "schema.default"
#define SANCTIFY_CFG_KEY_SCHEMA_CLOUD_PINYIN "schema.cloud_pinyin"
#define SANCTIFY_CFG_KEY_SCHEMA_LOCAL_PINYIN "schema.local_pinyin"

// 云端同步配置
#define SANCTIFY_CFG_KEY_SYNC_ENABLED        "sync.enabled"
#define SANCTIFY_CFG_KEY_SYNC_SERVER         "sync.server"
#define SANCTIFY_CFG_KEY_SYNC_TOKEN          "sync.token"

// 隐私配置
#define SANCTIFY_CFG_KEY_PRIVACY_ANALYTICS   "privacy.analytics"
#define SANCTIFY_CFG_KEY_PRIVACY_TELEMETRY   "privacy.telemetry"
#define SANCTIFY_CFG_KEY_PRIVACY_LOCAL_MODE  "privacy.local_mode"

// ─────────────────────────────────────────────────────────────
//  C++ 配置封装（链式 API）
// ─────────────────────────────────────────────────────────────

#ifdef __cplusplus

#include <string>
#include <optional>
#include <variant>

namespace sanctify::config {

class Node {
public:
  Node(SanctifyConfig* cfg, const std::string& prefix)
    : cfg_(cfg), prefix_(prefix) {}

  template <typename T>
  std::optional<T> get(const std::string& key) {
    std::string full = prefix_.empty() ? key : prefix_ + "." + key;
    if constexpr (std::is_same_v<T, bool>) {
      bool v; if (cfg_->get_bool(cfg_, full.c_str(), &v) == SANCTIFY_OK) return v;
    } else if constexpr (std::is_same_v<T, int64_t>) {
      int64_t v; if (cfg_->get_int(cfg_, full.c_str(), &v) == SANCTIFY_OK) return v;
    } else if constexpr (std::is_same_v<T, double>) {
      double v; if (cfg_->get_double(cfg_, full.c_str(), &v) == SANCTIFY_OK) return v;
    } else if constexpr (std::is_same_v<T, std::string>) {
      char buf[512];
      if (cfg_->get_str(cfg_, full.c_str(), buf, sizeof(buf)) == SANCTIFY_OK)
        return std::string(buf);
    }
    return std::nullopt;
  }

  Node section(const std::string& key) {
    std::string full = prefix_.empty() ? key : prefix_ + "." + key;
    return Node(cfg_, full);
  }

  template <typename T>
  void set(const std::string& key, T val) {
    std::string full = prefix_.empty() ? key : prefix_ + "." + key;
    if constexpr (std::is_same_v<T, bool>) cfg_->set_bool(cfg_, full.c_str(), val);
    else if constexpr (std::is_same_v<T, int64_t>) cfg_->set_int(cfg_, full.c_str(), val);
    else if constexpr (std::is_same_v<T, double>) cfg_->set_double(cfg_, full.c_str(), val);
    else if constexpr (std::is_same_v<T, std::string>) cfg_->set_str(cfg_, full.c_str(), val.c_str());
  }

  void save() { cfg_->save(cfg_); }

private:
  SanctifyConfig* cfg_;
  std::string prefix_;
};

}  // namespace sanctify::config

#endif  // __cplusplus
