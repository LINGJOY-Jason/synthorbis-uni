// =============================================================
// SynthOrbis UNI - macOS 音频实现（CoreAudio）
// platform/src/macos/audio_coreaudio.cc
// =============================================================

#include "platform/audio.h"
#include "platform/panic.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>
#include <cstring>
#include <cstdio>

// ── 设备属性监听 ───────────────────────────────────────────
static OSStatus
device_listener(AudioObjectID inObjectID,
                UInt32 inNumberAddresses,
                const AudioObjectPropertyAddress* inAddresses,
                void* inClientData) {
    SANCTIFY_UNUSED(inObjectID);
    SANCTIFY_UNUSED(inNumberAddresses);
    SANCTIFY_UNUSED(inAddresses);
    SANCTIFY_UNUSED(inClientData);
    // TODO: 触发设备变更回调
    return noErr;
}

// ── 获取默认设备 ID ────────────────────────────────────────
static AudioDeviceID
get_default_device_id(AudioObjectPropertySelector selector) {
    AudioDeviceID devID = kAudioObjectUnknown;
    AudioObjectPropertyAddress addr = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = sizeof(devID);
    OSStatus rc = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &addr, 0, nullptr, &size, &devID
    );
    return (rc == noErr) ? devID : kAudioObjectUnknown;
}

// ── 获取设备名称 ───────────────────────────────────────────
static void
get_device_name(AudioDeviceID devID, char* out_name, size_t max_len) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyDeviceNameCFString,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef name = nullptr;
    UInt32 size = sizeof(name);
    OSStatus rc = AudioObjectGetPropertyData(devID, &addr, 0, nullptr, &size, &name);
    if (rc == noErr && name) {
        CFStringGetCString(name, out_name, (CFIndex)max_len, kCFStringEncodingUTF8);
        CFRelease(name);
    } else {
        snprintf(out_name, max_len, "Device %u", devID);
    }
}

// ── 获取设备 UID ────────────────────────────────────────────
static void
get_device_uid(AudioDeviceID devID, char* out_uid, size_t max_len) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef uid = nullptr;
    UInt32 size = sizeof(uid);
    OSStatus rc = AudioObjectGetPropertyData(devID, &addr, 0, nullptr, &size, &uid);
    if (rc == noErr && uid) {
        CFStringGetCString(uid, out_uid, (CFIndex)max_len, kCFStringEncodingUTF8);
        CFRelease(uid);
    } else {
        snprintf(out_uid, max_len, "%u", devID);
    }
}

extern "C" {

SanctifyStatus sanctify_audio_create(SanctifyAudioManager** out_mgr) {
    SANCTIFY_UNUSED(out_mgr);
    return SANCTIFY_ERROR;  // 存根，待完整实现
}

SanctifyStatus
sanctify_audio_get_default_input(SanctifyAudioManager*,
                                  SanctifyAudioDeviceInfo* out_info) {
    AudioDeviceID devID = get_default_device_id(kAudioHardwarePropertyDefaultInputDevice);
    if (devID == kAudioObjectUnknown) return SANCTIFY_ERROR;

    get_device_uid(devID, out_info->id, sizeof(out_info->id));
    get_device_name(devID, out_info->name, sizeof(out_info->name));
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_get_default_output(SanctifyAudioManager*,
                                   SanctifyAudioDeviceInfo* out_info) {
    AudioDeviceID devID = get_default_device_id(kAudioHardwarePropertyDefaultOutputDevice);
    if (devID == kAudioObjectUnknown) return SANCTIFY_ERROR;

    get_device_uid(devID, out_info->id, sizeof(out_info->id));
    get_device_name(devID, out_info->name, sizeof(out_info->name));
    out_info->is_default = true;
    return SANCTIFY_OK;
}

SanctifyStatus
sanctify_audio_enumerate(SanctifyAudioManager*,
                         SanctifyAudioDeviceType type,
                         SanctifyAudioDeviceInfo* out_devs,
                         uint32_t max_devs,
                         uint32_t* out_count) {
    SANCTIFY_UNUSED(type);

    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus rc = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0, nullptr, &size);
    if (rc != noErr) return SANCTIFY_ERROR;

    uint32_t count = size / sizeof(AudioDeviceID);
    if (count == 0 || max_devs == 0) {
        *out_count = 0;
        return SANCTIFY_OK;
    }

    AudioDeviceID* devs = (AudioDeviceID*)malloc(size);
    if (!devs) return SANCTIFY_ERROR_MEMORY;

    rc = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0, nullptr, &size, devs);
    if (rc != noErr) {
        free(devs);
        return SANCTIFY_ERROR;
    }

    uint32_t written = 0;
    for (uint32_t i = 0; i < count && written < max_devs; ++i) {
        AudioDeviceID devID = devs[i];
        AudioObjectPropertyAddress scope_addr = {
            kAudioDevicePropertyStreams,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMain
        };
        UInt32 stream_size = 0;
        OSStatus sr = AudioObjectGetPropertyDataSize(devID, &scope_addr, 0, nullptr, &stream_size);

        get_device_uid(devID, out_devs[written].id, sizeof(out_devs[written].id));
        get_device_name(devID, out_devs[written].name, sizeof(out_devs[written].name));
        snprintf(out_devs[written].manufacturer, sizeof(out_devs[written].manufacturer), "Apple");
        out_devs[written].is_default = (devID == get_default_device_id(kAudioHardwarePropertyDefaultInputDevice));
        memset(out_devs[written].sample_rates, 0, sizeof(out_devs[written].sample_rates));
        out_devs[written].sample_rates[0] = 16000;
        out_devs[written].sample_rates[1] = 44100;
        out_devs[written].sample_rates[2] = 48000;
        out_devs[written].max_channels = 2;
        ++written;
    }

    free(devs);
    *out_count = written;
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
    return SANCTIFY_ERROR;  // 存根，待完整实现
}

SanctifyStatus sanctify_audio_play_sound(const char* sound_path) {
    SANCTIFY_UNUSED(sound_path);
    return SANCTIFY_ERROR;  // 存根
}

SanctifyStatus sanctify_audio_play_system_sound(int sound_id) {
    SANCTIFY_UNUSED(sound_id);
    SystemSoundID ssid = sound_id;
    OSStatus rc = AudioServicesPlaySystemSound(ssid);
    return (rc == noErr) ? SANCTIFY_OK : SANCTIFY_ERROR;
}

void sanctify_audio_destroy(SanctifyAudioManager*) {}

// ASR 音频录制存根
SanctifyStatus
sanctify_asr_audio_create(int, int, int, SanctifyASRAudio** out) {
    *out = nullptr;
    return SANCTIFY_ERROR;  // 存根，待完整实现
}

SanctifyStatus sanctify_asr_audio_start(SanctifyASRAudio*) { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_stop(SanctifyASRAudio*)  { return SANCTIFY_ERROR; }
SanctifyStatus sanctify_asr_audio_get_data(SanctifyASRAudio*, float**, uint32_t*) {
    return SANCTIFY_ERROR;
}
void sanctify_asr_audio_destroy(SanctifyASRAudio*) {}

}  // extern "C"
