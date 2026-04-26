#pragma once

/**
 * @file audio.h
 * @brief SynthOrbis UNI - 统一音频子系统抽象
 *
 * 抽象 WASAPI (Win) / CoreAudio (macOS) / ALSA (Linux) / OHOS Audio (鸿蒙车机)
 * 为输入法音效和语音输入提供统一接口。
 */

#include "platform/types.h"
#include "platform/panic.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  音频格式
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyAudioFormat {
    SANCTIFY_AUDIO_F32LE = 0,
    SANCTIFY_AUDIO_S16LE = 1,
    SANCTIFY_AUDIO_S16BE = 2,
    SANCTIFY_AUDIO_U8 = 3,
} SanctifyAudioFormat;

typedef struct SanctifyAudioSpec {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    SanctifyAudioFormat format;
} SanctifyAudioSpec;

#define SANCTIFY_AUDIO_SPEC_DEFAULT \
    { 16000, 1, 32, SANCTIFY_AUDIO_F32LE }

// ─────────────────────────────────────────────────────────────
//  音频设备类型
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyAudioDeviceType {
    SANCTIFY_AUDIO_INPUT = 0,
    SANCTIFY_AUDIO_OUTPUT = 1,
    SANCTIFY_AUDIO_LOOPBACK = 2,
} SanctifyAudioDeviceType;

typedef struct SanctifyAudioDeviceInfo {
    char id[128];
    char name[256];
    char manufacturer[128];
    bool is_default;
    uint32_t sample_rates[8];
    uint8_t max_channels;
} SanctifyAudioDeviceInfo;

// ─────────────────────────────────────────────────────────────
//  音频缓冲区回调
// ─────────────────────────────────────────────────────────────

typedef int (*SanctifyAudioCallback)(
    void* userdata,
    const float* data,
    uint32_t frames,
    const SanctifyAudioSpec* spec
);

// ─────────────────────────────────────────────────────────────
//  音频管理器
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyAudioManager SanctifyAudioManager;

typedef struct SanctifyAudioManagerVTable {
    void (*destroy)(SanctifyAudioManager* mgr);
} SanctifyAudioManagerVTable;

struct SanctifyAudioManager {
    const SanctifyAudioManagerVTable* vtable;
    void* platform_data;
};

// ─────────────────────────────────────────────────────────────
//  ASR 音频
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyASRAudio SanctifyASRAudio;

// ─────────────────────────────────────────────────────────────
//  C API
// ─────────────────────────────────────────────────────────────

/** 创建音频管理器 */
SANCTIFY_API SanctifyStatus
sanctify_audio_create(SanctifyAudioManager** out_mgr);

/** 销毁音频管理器 */
SANCTIFY_API void
sanctify_audio_destroy(SanctifyAudioManager* mgr);

/** ASR 音频创建 */
SANCTIFY_API SanctifyStatus
sanctify_asr_audio_create(int sample_rate, int channels, int max_seconds,
                          SanctifyASRAudio** out);

/** ASR 音频销毁 */
SANCTIFY_API void
sanctify_asr_audio_destroy(SanctifyASRAudio* a);

#ifdef __cplusplus
}  // extern "C"

// ─────────────────────────────────────────────────────────────
//  C++ RAII 封装
// ─────────────────────────────────────────────────────────────

#include <memory>

namespace sanctify {

class AudioManager {
public:
    AudioManager() : mgr_(nullptr) {}
    ~AudioManager() { destroy(); }

    bool create() {
        return sanctify_audio_create(&mgr_) == SANCTIFY_OK;
    }

    void destroy() {
        if (mgr_) {
            sanctify_audio_destroy(mgr_);
            mgr_ = nullptr;
        }
    }

    explicit operator bool() const { return mgr_ != nullptr; }

private:
    SanctifyAudioManager* mgr_;
};

}  // namespace sanctify

#endif  // __cplusplus
