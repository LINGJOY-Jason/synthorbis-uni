#pragma once

/**
 * @file config.h
 * @brief SynthOrbis UNI - 统一配置存储抽象
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
    SANCTIFY_CFG_NULL = 0,
    SANCTIFY_CFG_BOOL = 1,
    SANCTIFY_CFG_INT = 2,
    SANCTIFY_CFG_DOUBLE = 3,
    SANCTIFY_CFG_STRING = 4,
    SANCTIFY_CFG_LIST = 5,
    SANCTIFY_CFG_DICT = 6,
} SanctifyConfigValueType;

typedef struct SanctifyConfigValue {
    SanctifyConfigValueType type;
    union {
        bool bool_val;
        int64_t int_val;
        double double_val;
        char str_val[512];
    };
} SanctifyConfigValue;

// ─────────────────────────────────────────────────────────────
//  配置节点
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyConfigNode SanctifyConfigNode;
typedef struct SanctifyConfig SanctifyConfig;

struct SanctifyConfig {
    void* impl;

    SanctifyStatus (*get_bool)(SanctifyConfig*, const char* key, bool* out);
    SanctifyStatus (*get_int)(SanctifyConfig*, const char* key, int64_t* out);
    SanctifyStatus (*get_double)(SanctifyConfig*, const char* key, double* out);
    SanctifyStatus (*get_str)(SanctifyConfig*, const char* key, char* out, size_t max_len);

    SanctifyStatus (*set_bool)(SanctifyConfig*, const char* key, bool val);
    SanctifyStatus (*set_int)(SanctifyConfig*, const char* key, int64_t val);
    SanctifyStatus (*set_double)(SanctifyConfig*, const char* key, double val);
    SanctifyStatus (*set_str)(SanctifyConfig*, const char* key, const char* val);

    SanctifyStatus (*save)(SanctifyConfig*);
    void (*destroy)(SanctifyConfig*);
};

// ─────────────────────────────────────────────────────────────
//  C API
// ─────────────────────────────────────────────────────────────

/** 打开/创建配置文件 */
SANCTIFY_API SanctifyStatus
sanctify_config_open(const char* path, bool create_if_missing,
                     SanctifyConfig** out_cfg);

/** 销毁配置对象 */
SANCTIFY_API void
sanctify_config_destroy(SanctifyConfig* cfg);

// ─────────────────────────────────────────────────────────────
//  预定义配置键
// ─────────────────────────────────────────────────────────────

#define SANCTIFY_CFG_KEY_AI_ENABLED "ai.enabled"
#define SANCTIFY_CFG_KEY_ASR_ENABLED "asr.enabled"
#define SANCTIFY_CFG_KEY_UI_CANDIDATE_COUNT "ui.candidate_count"
#define SANCTIFY_CFG_KEY_UI_THEME "ui.theme"
#define SANCTIFY_CFG_KEY_SOUND_ENABLED "sound.enabled"

#ifdef __cplusplus
}  // extern "C"

// ─────────────────────────────────────────────────────────────
//  C++ 配置封装
// ─────────────────────────────────────────────────────────────

#include <string>
#include <optional>

namespace sanctify::config {

class Node {
public:
    Node(SanctifyConfig* cfg, const std::string& prefix = "")
        : cfg_(cfg), prefix_(prefix) {}

    template <typename T>
    std::optional<T> get(const std::string& key) {
        std::string full = prefix_.empty() ? key : prefix_ + "." + key;
        if constexpr (std::is_same_v<T, bool>) {
            bool v;
            if (cfg_->get_bool(cfg_, full.c_str(), &v) == SANCTIFY_OK) return v;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            int64_t v;
            if (cfg_->get_int(cfg_, full.c_str(), &v) == SANCTIFY_OK) return v;
        } else if constexpr (std::is_same_v<T, std::string>) {
            char buf[512];
            if (cfg_->get_str(cfg_, full.c_str(), buf, sizeof(buf)) == SANCTIFY_OK)
                return std::string(buf);
        }
        return std::nullopt;
    }

    void save() { cfg_->save(cfg_); }

private:
    SanctifyConfig* cfg_;
    std::string prefix_;
};

}  // namespace sanctify::config

#endif  // __cplusplus
