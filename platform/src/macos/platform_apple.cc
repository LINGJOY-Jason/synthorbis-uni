// =============================================================
// SynthOrbis UNI - macOS 平台实现
// platform/src/macos/platform_apple.cc
// =============================================================

#include "platform.h"
#include "platform/types.h"
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <cstdlib>
#include <cstring>

static char g_exe_dir[1024] = {0};
static char g_config_dir[1024] = {0};
static char g_log_dir[1024] = {0};
static bool g_initialized = false;

extern "C" {

int sanctify_init(void) {
    if (g_initialized) return 0;

    // 获取可执行文件路径
    char exe_path[1024] = {0};
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        strncpy(g_exe_dir, dirname(exe_path), sizeof(g_exe_dir) - 1);
    } else {
        snprintf(g_exe_dir, sizeof(g_exe_dir), "/Applications/sanctify.app/Contents/MacOS");
    }

    // macOS UserDefaults 配置目录
    const char* home = getenv("HOME");
    snprintf(g_config_dir, sizeof(g_config_dir), "%s/Library/Application Support/sanctify",
             home ? home : "/tmp");
    snprintf(g_log_dir, sizeof(g_log_dir), "%s/Logs", g_config_dir);

    g_initialized = true;
    return 0;
}

void sanctify_shutdown(void) {
    g_initialized = false;
}

const char* sanctify_platform_name(void) {
    return "macOS";
}

const char* sanctify_rime_version(void) {
    return "1.16.1";  // 存根
}

const char* sanctify_build_timestamp(void) {
    return __DATE__ " " __TIME__;
}

int sanctify_is_xinchuang(void) {
    return 0;  // macOS 一般不是信创平台
}

char* sanctify_get_exe_dir(void) {
    return strdup(g_exe_dir);
}

char* sanctify_get_config_dir(void) {
    return strdup(g_config_dir);
}

char* sanctify_get_log_path(const char* module_name) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/%s.log",
             g_log_dir, module_name ? module_name : "sanctify");
    return path;
}

char* sanctify_get_temp_dir(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "/tmp/sanctify-XXXXXX");
    char* ret = mkdtemp(path);
    return ret ? strdup(ret) : nullptr;
}

void sanctify_log(int level, const char* module, const char* fmt, ...) {
    SANCTIFY_UNUSED(level); SANCTIFY_UNUSED(module); SANCTIFY_UNUSED(fmt);
}

int sanctify_cpu_count(void) {
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &size, nullptr, 0) == 0) {
        return count;
    }
    return 1;
}

uint64_t sanctify_total_memory(void) {
    uint64_t mem;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == 0) {
        return mem;
    }
    return 0;
}

}  // extern "C"
