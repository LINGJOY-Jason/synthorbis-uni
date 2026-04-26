# =============================================================
# SynthOrbis UNI — Linux 配置实现（XDG）
# platform/src/linux/config_xdg.cc
# =============================================================

#include "platform/config.h"
#include "platform/platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static char g_config_path[1024] = {0};

static void ensure_config_dir(void) {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (!xdg || !xdg[0]) {
        const char* home = getenv("HOME");
        snprintf(g_config_path, sizeof(g_config_path),
                 "%s/.config/sanctify", home ? home : "/tmp");
    } else {
        snprintf(g_config_path, sizeof(g_config_path), "%s/sanctify", xdg);
    }
    mkdir(g_config_path, 0755);
}

extern "C" {

SanctifyStatus
sanctify_config_open(const char* path, bool create_if_missing,
                     SanctifyConfig** out_cfg) {
    SANCTIFY_UNUSED(path);
    SANCTIFY_UNUSED(create_if_missing);
    SANCTIFY_UNUSED(out_cfg);
    return SANCTIFY_ERROR;  // 存根，待集成 libyaml / json-c
}

SanctifyStatus
sanctify_config_open_user(const char* filename,
                          bool create_if_missing,
                          SanctifyConfig** out_cfg) {
    SANCTIFY_UNUSED(filename);
    SANCTIFY_UNUSED(create_if_missing);
    SANCTIFY_UNUSED(out_cfg);
    return SANCTIFY_ERROR;  // 存根
}

SanctifyStatus
sanctify_config_get_default(const char* key, SanctifyConfigValue* out) {
    // 硬编码默认值
    if (strcmp(key, SANCTIFY_CFG_KEY_UI_CANDIDATE_COUNT) == 0) {
        out->type = SANCTIFY_CFG_INT;
        out->int_val = 9;
    } else if (strcmp(key, SANCTIFY_CFG_KEY_UI_THEME) == 0) {
        out->type = SANCTIFY_CFG_STRING;
        strcpy(out->str_val, "dark");
    } else if (strcmp(key, SANCTIFY_CFG_KEY_AI_ENABLED) == 0) {
        out->type = SANCTIFY_CFG_BOOL;
        out->bool_val = true;
    } else {
        return SANCTIFY_ERROR;
    }
    return SANCTIFY_OK;
}

void sanctify_config_on_changed(const char*, SanctifyConfigChangedCallback, void*) {}

void sanctify_config_destroy(SanctifyConfig* cfg) {
    SANCTIFY_UNUSED(cfg);
}

}  // extern "C"
