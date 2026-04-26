# =============================================================
# SynthOrbis UNI — HarmonyOS 配置实现（Preferences）
# platform/src/vehicle/config_preferences.cc
# =============================================================

#include "platform/config.h"
#include <cstring>

extern "C" {

SanctifyStatus
sanctify_config_open(const char* path, bool create, SanctifyConfig** out) {
    SANCTIFY_UNUSED(path); SANCTIFY_UNUSED(create); SANCTIFY_UNUSED(out);
    return SANCTIFY_ERROR;  // 存根，待集成 OHOS Preferences API
}

SanctifyStatus
sanctify_config_open_user(const char* filename,
                          bool create_if_missing, SanctifyConfig** out_cfg) {
    SANCTIFY_UNUSED(filename); SANCTIFY_UNUSED(create_if_missing);
    SANCTIFY_UNUSED(out_cfg);
    return SANCTIFY_ERROR;
}

SanctifyStatus
sanctify_config_get_default(const char* key, SanctifyConfigValue* out) {
    if (strcmp(key, SANCTIFY_CFG_KEY_UI_CANDIDATE_COUNT) == 0) {
        out->type = SANCTIFY_CFG_INT; out->int_val = 5;  // 车机默认5候选
    } else if (strcmp(key, SANCTIFY_CFG_KEY_UI_THEME) == 0) {
        out->type = SANCTIFY_CFG_STRING; strcpy(out->str_val, "dark");
    } else if (strcmp(key, SANCTIFY_CFG_KEY_AI_ENABLED) == 0) {
        out->type = SANCTIFY_CFG_BOOL; out->bool_val = true;
    } else {
        return SANCTIFY_ERROR;
    }
    return SANCTIFY_OK;
}

void sanctify_config_on_changed(const char*, SanctifyConfigChangedCallback, void*) {}
void sanctify_config_destroy(SanctifyConfig*) {}

}  // extern "C"
