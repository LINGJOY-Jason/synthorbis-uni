# =============================================================
# SynthOrbis UNI — Windows 音频实现（WASAPI）
# platform/src/windows/audio_wasapi.cc
# =============================================================

#include "platform/audio.h"
#include "platform/panic.h"

extern "C" {

SanctifyStatus sanctify_audio_create(SanctifyAudioManager** out_mgr) {
    SANCTIFY_UNUSED(out_mgr);
    return SANCTIFY_ERROR;  // 存根
}

SanctifyStatus
sanctify_audio_get_default_input(SanctifyAudioManager*,
                                  SanctifyAudioDeviceInfo* out_info) {
    snprintf(out_info->id, sizeof(out_info->id), "{0.0.0.00000000}.{default}");
    snprintf(out_info->name, sizeof(out_info->name), "Default Input Device");
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_get_default_output(SanctifyAudioManager*,
                                   SanctifyAudioDeviceInfo* out_info) {
    snprintf(out_info->id, sizeof(out_info->id), "{0.0.0.00000000}.{default}");
    snprintf(out_info->name, sizeof(out_info->name), "Default Output Device");
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_enumerate(SanctifyAudioManager*,
                          SanctifyAudioDeviceType,
                          SanctifyAudioDeviceInfo* out_devs,
                          uint32_t max_devs, uint32_t* out_count) {
    if (max_devs < 1) return SANCTIFY_ERROR;
    sanctify_audio_get_default_input(nullptr, out_devs);
    *out_count = 1;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_open_input(SanctifyAudioManager*,
                           const char* device_id,
                           const SanctifyAudioSpec* spec,
                           SanctifyAudioCallback callback,
                           void* userdata, void** out_stream) {
    SANCTIFY_UNUSED(device_id); SANCTIFY_UNUSED(spec);
    SANCTIFY_UNUSED(callback); SANCTIFY_UNUSED(userdata);
    SANCTIFY_UNUSED(out_stream);
    return SANCTIFY_ERROR;
}

SanctifyStatus sanctify_audio_play_sound(const char* path) {
    SANCTIFY_UNUSED(path);
    return SANCTIFY_ERROR;
}

SanctifyStatus sanctify_audio_play_system_sound(int sound_id) {
    SANCTIFY_UNUSED(sound_id);
    return SANCTIFY_ERROR;
}

void sanctify_audio_destroy(SanctifyAudioManager*) {}

SanctifyStatus
sanctify_asr_audio_create(int, int, int, SanctifyASRAudio** out) {
    *out = nullptr; return SANCTIFY_ERROR;
}
SanctifyStatus sanctify_asr_audio_start(SanctifyASRAudio*) { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_stop(SanctifyASRAudio*)  { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_get_data(SanctifyASRAudio*, float**, uint32_t*) {
    return SANCTIFY_ERROR;
}
void sanctify_asr_audio_destroy(SanctifyASRAudio*) {}

}  // extern "C"
