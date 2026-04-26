// SynthOrbis Input Adapter — 输入适配器实现

#include "synthorbis/input_adapter.h"

#include <cstring>

// ─────────────────────────────────────────────────────────
// 键码映射表
// ─────────────────────────────────────────────────────────

struct KeyMapping {
    int platform_key;
    int rime_key;
};

static const KeyMapping g_key_mappings[] = {
    // 功能键
    {0x08, 0xff08},  // BackSpace
    {0x09, 0xff09},  // Tab
    {0x0D, 0xff0d},  // Enter
    {0x1B, 0xff1b},  // Escape
    {0x20, 0x20},    // Space
    {0x21, 0xff55},  // PageUp
    {0x22, 0xff56},  // PageDown
    {0x23, 0xff57},  // End
    {0x24, 0xff50},  // Home
    {0x25, 0xff51},  // Left
    {0x26, 0xff52},  // Up
    {0x27, 0xff53},  // Right
    {0x28, 0xff54},  // Down
    {0x2D, 0xff63},  // Insert
    {0x2E, 0xffff},  // Delete (RIME uses 0xffff)
};

// ─────────────────────────────────────────────────────────
// 内部数据结构
// ─────────────────────────────────────────────────────────

struct SynthInputAdapter {
    int active;
    void* engine;  // 关联的 SynthEngine
};

// ─────────────────────────────────────────────────────────
// C API 实现
// ─────────────────────────────────────────────────────────

SynthInputAdapter* synth_input_adapter_create(void) {
    SynthInputAdapter* adapter = new SynthInputAdapter;
    adapter->active = 0;
    adapter->engine = nullptr;
    return adapter;
}

void synth_input_adapter_destroy(SynthInputAdapter* adapter) {
    delete adapter;
}

SynthInputResult synth_input_adapter_process_key(
    SynthInputAdapter* adapter,
    int keycode,
    int modifier,
    int is_release) {

    if (!adapter || !adapter->active) {
        return SYNTH_INPUT_IGNORED;
    }

    // 处理修饰键
    int rime_modifier = 0;
    if (modifier & SYNTH_KEY_MOD_SHIFT) rime_modifier |= 0x1;
    if (modifier & SYNTH_KEY_MOD_CONTROL) rime_modifier |= 0x4;
    if (modifier & SYNTH_KEY_MOD_ALT) rime_modifier |= 0x8;

    // 映射键码
    int rime_keycode;
    if (!synth_input_adapter_map_keycode(adapter, keycode, modifier,
                                          &rime_keycode, &rime_modifier)) {
        return SYNTH_INPUT_NOT_HANDLED;
    }

    // TODO: 调用 RIME 引擎处理按键
    // rime_engine_process_key(adapter->engine, rime_keycode, rime_modifier);

    return SYNTH_INPUT_PROCESSED;
}

SynthInputResult synth_input_adapter_process_text(
    SynthInputAdapter* adapter,
    const char* text) {

    if (!adapter || !adapter->active || !text) {
        return SYNTH_INPUT_IGNORED;
    }

    // 处理文本输入
    // TODO: 将文本传递给 RIME 引擎

    return SYNTH_INPUT_PROCESSED;
}

SynthInputResult synth_input_adapter_process_voice(
    SynthInputAdapter* adapter,
    const char* text,
    float confidence) {

    if (!adapter || !adapter->active || !text) {
        return SYNTH_INPUT_IGNORED;
    }

    // 验证语音置信度
    if (confidence < 0.5f) {
        return SYNTH_INPUT_IGNORED;
    }

    // TODO: 调用云端 AI 处理语音输入

    return SYNTH_INPUT_PROCESSED;
}

int synth_input_adapter_map_keycode(
    SynthInputAdapter* adapter,
    int platform_keycode,
    int platform_modifier,
    int* rime_keycode,
    int* rime_modifier) {

    if (!adapter || !rime_keycode || !rime_modifier) {
        return 0;
    }

    // 查找映射
    for (const auto& mapping : g_key_mappings) {
        if (mapping.platform_key == platform_keycode) {
            *rime_keycode = mapping.rime_key;
            break;
        }
    }

    // 如果没有找到映射，使用原始键码
    if (*rime_keycode == 0) {
        *rime_keycode = platform_keycode;
    }

    // 映射修饰键
    *rime_modifier = 0;
    if (platform_modifier & SYNTH_KEY_MOD_SHIFT) {
        *rime_modifier |= 0x1;  // ShiftMask
    }
    if (platform_modifier & SYNTH_KEY_MOD_CONTROL) {
        *rime_modifier |= 0x4;  // ControlMask
    }
    if (platform_modifier & SYNTH_KEY_MOD_ALT) {
        *rime_modifier |= 0x8;  // AltMask
    }

    return 1;
}

void synth_input_adapter_set_active(SynthInputAdapter* adapter, int active) {
    if (adapter) {
        adapter->active = active;
    }
}
