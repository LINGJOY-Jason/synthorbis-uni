// =============================================================
// SynthOrbis UNI - Linux 音频实现（ALSA）
// platform/src/linux/audio_alsa.cc
// =============================================================

#include "platform.h"
#include "platform/audio.h"

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <cstring>
#include <cstdio>

struct SanctifyALSAData {
    snd_pcm_t*           pcm;
    pthread_t            thread;
    SanctifyAudioCallback callback;
    void*                userdata;
    SanctifyAudioSpec    spec;
    volatile bool        running;
};

// ── 录制线程 ───────────────────────────────────────────────
static void* alsarec_thread(void* arg) {
    SanctifyALSAData* d = (SanctifyALSAData*)arg;

    const int period_frames = 1024;
    float buffer[period_frames * d->spec.channels];
    snd_pcm_format_t fmt = SND_PCM_FORMAT_FLOAT;

    while (d->running) {
        int rc = snd_pcm_readi(d->pcm, buffer, period_frames);
        if (rc == -EAGAIN) continue;
        if (rc < 0) {
            fprintf(stderr, "[ALSA] Read error: %s\n", snd_strerror(rc));
            snd_pcm_prepare(d->pcm);
            continue;
        }

        int ret = d->callback(d->userdata, buffer, (uint32_t)rc, &d->spec);
        if (ret != 0) break;
    }
    return nullptr;
}

extern "C" {

SanctifyStatus sanctify_audio_create(SanctifyAudioManager** out_mgr) {
    SANCTIFY_UNUSED(out_mgr);
    return SANCTIFY_ERROR;  // 存根，待完整实现
}

SanctifyStatus
sanctify_audio_get_default_input(SanctifyAudioManager*,
                                  SanctifyAudioDeviceInfo* out_info) {
    snprintf(out_info->id, sizeof(out_info->id), "default");
    snprintf(out_info->name, sizeof(out_info->name), "Default Microphone");
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_get_default_output(SanctifyAudioManager*,
                                   SanctifyAudioDeviceInfo* out_info) {
    snprintf(out_info->id, sizeof(out_info->id), "default");
    snprintf(out_info->name, sizeof(out_info->name), "Default Speaker");
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_enumerate(SanctifyAudioManager*,
                          SanctifyAudioDeviceType,
                          SanctifyAudioDeviceInfo* out_devs,
                          uint32_t max_devs,
                          uint32_t* out_count) {
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
                           void* userdata,
                           void** out_stream) {
    SANCTIFY_UNUSED(device_id);
    SANCTIFY_UNUSED(spec);
    SANCTIFY_UNUSED(callback);
    SANCTIFY_UNUSED(userdata);
    SANCTIFY_UNUSED(out_stream);
    return SANCTIFY_ERROR;  // 存根
}

SanctifyStatus sanctify_audio_play_sound(const char* sound_path) {
    SANCTIFY_UNUSED(sound_path);
    return SANCTIFY_ERROR;  // 存根
}

SanctifyStatus sanctify_audio_play_system_sound(int sound_id) {
    SANCTIFY_UNUSED(sound_id);
    return SANCTIFY_ERROR;  // 存根
}

void sanctify_audio_destroy(SanctifyAudioManager* mgr) {
    SANCTIFY_UNUSED(mgr);
}

// ASR 音频录制存根
SanctifyStatus
sanctify_asr_audio_create(int, int, int, SanctifyASRAudio** out) {
    *out = nullptr;
    return SANCTIFY_ERROR;  // 存根
}
SanctifyStatus sanctify_asr_audio_start(SanctifyASRAudio*) { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_stop(SanctifyASRAudio*)  { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_get_data(SanctifyASRAudio*,
                                            float**, uint32_t*) {
    return SANCTIFY_ERROR;
}
void sanctify_asr_audio_destroy(SanctifyASRAudio*) {}

}  // extern "C"
