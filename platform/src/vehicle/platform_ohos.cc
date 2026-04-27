// =============================================================
// SynthOrbis UNI - HarmonyOS 车机平台实现
// platform/src/vehicle/platform_ohos.cc
// =============================================================

#include "platform.h"
#include "platform/types.h"

extern "C" {

int sanctify_init(void) {
    return 0;
}

void sanctify_shutdown(void) {}

const char* sanctify_platform_name(void) {
    return "HarmonyOS (Vehicle)";
}

const char* sanctify_rime_version(void) {
    return "1.16.1";
}

const char* sanctify_build_timestamp(void) {
    return __DATE__ " " __TIME__;
}

int sanctify_is_xinchuang(void) {
    return 1;  // 鸿蒙车机属于国产化平台
}

char* sanctify_get_exe_dir(void) {
    return strdup("/data/data/com.sanctify.ime");
}

char* sanctify_get_config_dir(void) {
    return strdup("/data/data/com.sanctify.ime/config");
}

char* sanctify_get_log_path(const char* module_name) {
    char path[1024];
    snprintf(path, sizeof(path), "/data/data/com.sanctify.ime/logs/%s.log",
             module_name ? module_name : "sanctify");
    return strdup(path);
}

char* sanctify_get_temp_dir(void) {
    return strdup("/data/data/com.sanctify.ime/cache");
}

void sanctify_log(int level, const char* module, const char* fmt, ...) {
    SANCTIFY_UNUSED(level); SANCTIFY_UNUSED(module); SANCTIFY_UNUSED(fmt);
}

int sanctify_cpu_count(void) {
    // 从 /proc/cpuinfo 读取或调用 OHOS_GetCpuCount()
    return 4;  // 存根
}

uint64_t sanctify_total_memory(void) {
    return 4ULL * 1024 * 1024 * 1024;  // 4GB，存根
}

}  // extern "C"
