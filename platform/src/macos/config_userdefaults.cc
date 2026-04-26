# =============================================================
# SynthOrbis UNI — macOS 配置实现（UserDefaults）
# platform/src/macos/config_userdefaults.cc
# =============================================================

#include "platform/config.h"
#include "platform/platform.h"
#include <Foundation/Foundation.h>
#include <cstdlib>
#include <cstring>

// ── CF / NS 桥接辅助 ───────────────────────────────────────
static NSUserDefaults* get_defaults(void) {
    return [NSUserDefaults standardUserDefaults];
}

extern "C" {

SanctifyStatus
sanctify_config_open(const char* path, bool create, SanctifyConfig** out_cfg) {
    SANCTIFY_UNUSED(path);
    SANCTIFY_UNUSED(create);
    SANCTIFY_UNUSED(out_cfg);
    return SANCTIFY_ERROR;  // 存根，待集成 NSUserDefaults
}

SanctifyStatus
sanctify_config_open_user(const char* filename,
                          bool create_if_missing,
                          SanctifyConfig** out_cfg) {
    SANCTIFY_UNUSED(filename);
    SANCTIFY_UNUSED(create_if_missing);
    SANCTIFY_UNUSED(out_cfg);
    return SANCTIFY_ERROR;  // 存根，待集成 NSUserDefaults
}

SanctifyStatus
sanctify_config_get_default(const char* key, SanctifyConfigValue* out) {
    // 从 NSUserDefaults 读取，未设置时返回默认值
    NSUserDefaults* ud = get_defaults();
    id value = [ud objectForKey:[NSString stringWithUTF8String:key]];

    if (!value) {
        // 回退到硬编码默认值
        if (strcmp(key, SANCTIFY_CFG_KEY_UI_CANDIDATE_COUNT) == 0) {
            out->type = SANCTIFY_CFG_INT; out->int_val = 9;
        } else if (strcmp(key, SANCTIFY_CFG_KEY_UI_THEME) == 0) {
            out->type = SANCTIFY_CFG_STRING; strcpy(out->str_val, "dark");
        } else if (strcmp(key, SANCTIFY_CFG_KEY_AI_ENABLED) == 0) {
            out->type = SANCTIFY_CFG_BOOL; out->bool_val = true;
        } else {
            return SANCTIFY_ERROR;
        }
        return SANCTIFY_OK;
    }

    if ([value isKindOfClass:[NSNumber class]]) {
        NSNumber* num = (NSNumber*)value;
        const char* type = [num objCType];
        if (strcmp(type, @encode(BOOL)) == 0 || strcmp(type, @encode(int)) == 0) {
            out->type = SANCTIFY_CFG_INT;
            out->int_val = [num intValue];
        } else {
            out->type = SANCTIFY_CFG_DOUBLE;
            out->double_val = [num doubleValue];
        }
    } else if ([value isKindOfClass:[NSString class]]) {
        out->type = SANCTIFY_CFG_STRING;
        strncpy(out->str_val, [value UTF8String], sizeof(out->str_val) - 1);
    } else {
        return SANCTIFY_ERROR;
    }
    return SANCTIFY_OK;
}

void
sanctify_config_on_changed(const char* key_pattern,
                            SanctifyConfigChangedCallback cb,
                            void* userdata) {
    SANCTIFY_UNUSED(key_pattern);
    SANCTIFY_UNUSED(cb);
    SANCTIFY_UNUSED(userdata);
    // TODO: 使用 NSUserDefaults 的 KVO 或 Darwin Notify 监控变更
}

void
sanctify_config_destroy(SanctifyConfig* cfg) {
    SANCTIFY_UNUSED(cfg);
}

}  // extern "C"
