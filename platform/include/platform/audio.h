#pragma once

/**
 * @file audio.h
 * @brief SynthOrbis UNI — 统一音频子系统抽象
 *
 * 抽象 WASAPI (Win) / CoreAudio (macOS) / ALSA (Linux) / OHOS Audio (鸿蒙车机)
 * 为输入法音效和语音输入提供统一接口。
 *
 * @note 核心输入法引擎不依赖此模块，仅在需要音效/ASR 时实例化。
 */

#include "platform/types.h"
#include "platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
//  音频格式
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyAudioFormat {
  SANCTIFY_AUDIO_F32LE = 0,   // Float32 Little-Endian（跨平台推荐）
  SANCTIFY_AUDIO_S16LE = 1,   // Int16 PCM
  SANCTIFY_AUDIO_S16BE = 2,   // Int16 PCM Big-Endian
  SANCTIFY_AUDIO_U8     = 3, // Unsigned 8-bit PCM
} SanctifyAudioFormat;

typedef struct SanctifyAudioSpec {
  uint32_t sample_rate;    // 采样率：8000 / 16000 / 44100 / 48000
  uint8_t  channels;       // 声道数：1 (mono) / 2 (stereo)
  uint8_t  bits_per_sample; // 位深：8 / 16 / 32
  SanctifyAudioFormat format;
} SanctifyAudioSpec;

#define SANCTIFY_AUDIO_SPEC_DEFAULT \
  { 16000, 1, 32, SANCTIFY_AUDIO_F32LE }

// ─────────────────────────────────────────────────────────────
//  音频设备类型
// ─────────────────────────────────────────────────────────────

typedef enum SanctifyAudioDeviceType {
  SANCTIFY_AUDIO_INPUT   = 0,   // 麦克风 / ASR 输入
  SANCTIFY_AUDIO_OUTPUT  = 1,   // 扬声器 / 音效输出
  SANCTIFY_AUDIO_LOOPBACK = 2   // 监听（回环）
} SanctifyAudioDeviceType;

typedef struct SanctifyAudioDeviceInfo {
  char     id[128];        // 设备唯一标识
  char     name[256];      // 设备显示名称（UTF-8）
  char     manufacturer[128]; // 厂商
  bool     is_default;    // 是否为系统默认设备
  uint32_t sample_rates[8]; // 支持的采样率列表（0=终止）
  uint8_t  max_channels;
} SanctifyAudioDeviceInfo;

// ─────────────────────────────────────────────────────────────
//  音频缓冲区回调类型
// ─────────────────────────────────────────────────────────────

/**
 * 音频数据回调（采集满一帧时触发）
 * @param userdata 用户数据
 * @param data     PCM 数据缓冲区
 * @param frames   帧数
 * @param spec     音频格式
 * @return 0=继续，-1=停止采集
 */
typedef int (*SanctifyAudioCallback)(
    void* userdata,
    const float* data,
    uint32_t frames,
    const SanctifyAudioSpec* spec
);

// ─────────────────────────────────────────────────────────────
//  音频设备管理器（统一接口）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyAudioManager SanctifyAudioManager;

typedef struct SanctifyAudioManagerVTable {
  SanctifyStatus (*enumerate)(
      SanctifyAudioManager* mgr,
      SanctifyAudioDeviceType type,
      SanctifyAudioDeviceInfo* out_devs,
      uint32_t max_devs,
      uint32_t* out_count
  );

  SanctifyStatus (*open_input)(
      SanctifyAudioManager* mgr,
      const char* device_id,
      const SanctifyAudioSpec* spec,
      SanctifyAudioCallback callback,
      void* userdata,
      void** out_stream
  );

  SanctifyStatus (*open_output)(
      SanctifyAudioManager* mgr,
      const char* device_id,
      const SanctifyAudioSpec* spec,
      void** out_stream
  );

  SanctifyStatus (*start)(void* stream);
  SanctifyStatus (*stop)(void* stream);
  SanctifyStatus (*close)(void* stream);

  void (*destroy)(SanctifyAudioManager* mgr);
} SanctifyAudioManagerVTable;

struct SanctifyAudioManager {
  const SanctifyAudioManagerVTable* vtable;
  void* platform_data;
};

// ─────────────────────────────────────────────────────────────
//  统一 C API
// ─────────────────────────────────────────────────────────────

/** 创建平台音频管理器（自动选择后端） */
SANCTIFY_API SanctifyStatus
sanctify_audio_create(SanctifyAudioManager** out_mgr);

/** 获取默认输入设备信息 */
SANCTIFY_API SanctifyStatus
sanctify_audio_get_default_input(SanctifyAudioManager* mgr,
                                  SanctifyAudioDeviceInfo* out_info);

/** 获取默认输出设备信息 */
SANCTIFY_API SanctifyStatus
sanctify_audio_get_default_output(SanctifyAudioManager* mgr,
                                    SanctifyAudioDeviceInfo* out_info);

/** 列举所有音频设备 */
SANCTIFY_API SanctifyStatus
sanctify_audio_enumerate(SanctifyAudioManager* mgr,
                          SanctifyAudioDeviceType type,
                          SanctifyAudioDeviceInfo* out_devs,
                          uint32_t max_devs,
                          uint32_t* out_count);

/** 打开音频输入流（麦克风采集） */
SANCTIFY_API SanctifyStatus
sanctify_audio_open_input(SanctifyAudioManager* mgr,
                           const char* device_id,
                           const SanctifyAudioSpec* spec,
                           SanctifyAudioCallback callback,
                           void* userdata,
                           void** out_stream);

/** 播放 PCM 音效（短音频反馈） */
SANCTIFY_API SanctifyStatus
sanctify_audio_play_sound(const char* sound_path);

/** 播放内置系统音效（输入法按键音等） */
SANCTIFY_API SanctifyStatus
sanctify_audio_play_system_sound(int sound_id);

/** 销毁音频管理器 */
SANCTIFY_API void
sanctify_audio_destroy(SanctifyAudioManager* mgr);

/** 音频事件 ID（预定义） */
enum SanctifySystemSoundID {
  SANCTIFY_SOUND_KEY_CLICK     = 1001, // 按键音
  SANCTIFY_SOUND_KEY_LONGPRESS = 1002, // 长按音
  SANCTIFY_SOUND_ENGAGE        = 1003, // 切换输入法音效
  SANCTIFY_SOUND_CANDIDATE     = 1004, // 候选词选择音
};

// ─────────────────────────────────────────────────────────────
//  ASR 音频助手（封装采集 → 预处理）
// ─────────────────────────────────────────────────────────────

typedef struct SanctifyASRAudio SanctifyASRAudio;

/**
 * 创建 ASR 音频录制器
 * @param sample_rate 采样率（推荐 16000）
 * @param channels    声道数（推荐 1）
 * @param max_seconds 最大录制时长（秒），0=不限
 */
SANCTIFY_API SanctifyStatus
sanctify_asr_audio_create(int sample_rate, int channels, int max_seconds,
                          SanctifyASRAudio** out);

/** 开始录制（异步，采集到回调） */
SANCTIFY_API SanctifyStatus
sanctify_asr_audio_start(SanctifyASRAudio* a);

/** 停止录制 */
SANCTIFY_API SanctifyStatus
sanctify_asr_audio_stop(SanctifyASRAudio* a);

/** 获取录制的数据（PCM Float32，调用者不负责释放） */
SANCTIFY_API SanctifyStatus
sanctify_asr_audio_get_data(SanctifyASRAudio* a,
                             float** out_data,
                             uint32_t* out_frames);

/** 释放录制器 */
SANCTIFY_API void
sanctify_asr_audio_destroy(SanctifyASRAudio* a);

#ifdef __cplusplus
}  // extern "C"

// ─────────────────────────────────────────────────────────────
//  C++ RAII 封装
// ─────────────────────────────────────────────────────────────

#include <memory>
#include <functional>

namespace sanctify {

class AudioStream {
public:
  AudioStream() : stream_(nullptr), mgr_(nullptr) {}
  ~AudioStream() { close(); }

  bool open_input(SanctifyAudioManager* mgr,
                  const char* device_id,
                  const SanctifyAudioSpec& spec,
                  SanctifyAudioCallback cb,
                  void* ud) {
    return sanctify_audio_open_input(mgr, device_id, &spec, cb, ud,
                                     &stream_) == SANCTIFY_OK;
  }

  bool start() { return sanctify_audio_start(stream_) == SANCTIFY_OK; }
  bool stop()  { return sanctify_audio_stop(stream_)  == SANCTIFY_OK; }
  void close() { if (stream_) sanctify_audio_close(stream_); stream_ = nullptr; }

  void* stream() const { return stream_; }
  explicit operator bool() const { return stream_ != nullptr; }

private:
  void* stream_;
  SanctifyAudioManager* mgr_;
};

// ASR 录制封装
class ASRAudio {
public:
  ASRAudio(int sample_rate = 16000, int channels = 1, int max_sec = 0) {
    sanctify_asr_audio_create(sample_rate, channels, max_sec, &audio_);
  }
  ~ASRAudio() { if (audio_) sanctify_asr_audio_destroy(audio_); }

  bool start() { return sanctify_asr_audio_start(audio_) == SANCTIFY_OK; }
  bool stop()  { return sanctify_asr_audio_stop(audio_)  == SANCTIFY_OK; }

  std::pair<float*, uint32_t> get_data() {
    float* data = nullptr;
    uint32_t frames = 0;
    sanctify_asr_audio_get_data(audio_, &data, &frames);
    return {data, frames};
  }

  explicit operator bool() const { return audio_ != nullptr; }

private:
  SanctifyASRAudio* audio_;
};

}  // namespace sanctify

#endif  // __cplusplus
