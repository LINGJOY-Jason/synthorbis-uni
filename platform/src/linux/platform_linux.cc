# =============================================================
# SynthOrbis UNI — Linux 平台实现
# platform/src/linux/platform_linux.cc
# =============================================================

#include "platform/platform.h"
#include "platform/types.h"
#include "platform/panic.h"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <libgen.h>

static char g_exe_dir[1024] = {0};
static char g_config_dir[1024] = {0};
static char g_log_dir[1024] = {0};
static bool g_initialized = false;

// 获取 RIME 版本（从 librime 动态库获取）
static const char* get_rime_version_impl() {
    static char version[64] = {0};
    if (version[0]) return version;

    void* handle = dlopen("librime.so.1", RTLD_NOLOAD);
    if (!handle) handle = dlopen("librime.so", RTLD_NOLOAD);
    if (handle) {
        auto* ver_fn = (const char* (*)())dlsym(handle, "RimeGetVersion");
        if (ver_fn) {
            const char* v = ver_fn();
            if (v) snprintf(version, sizeof(version), "%s", v);
        }
        dlclose(handle);
    }
    if (!version[0]) {
        snprintf(version, sizeof(version), "1.16.1");
    }
    return version;
}

extern "C" {

int sanctify_init(void) {
    if (g_initialized) return 0;

    // 获取可执行文件路径
    char exe_path[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        strncpy(g_exe_dir, dirname(exe_path), sizeof(g_exe_dir) - 1);
    } else {
        snprintf(g_exe_dir, sizeof(g_exe_dir), "/usr/local");
    }

    // XDG 配置目录
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (!xdg_config || !xdg_config[0]) {
        snprintf(g_config_dir, sizeof(g_config_dir), "%s/.config/sanctify",
                 getenv("HOME") ? getenv("HOME") : "/tmp");
    } else {
        snprintf(g_config_dir, sizeof(g_config_dir), "%s/sanctify", xdg_config);
    }

    // 日志目录
    snprintf(g_log_dir, sizeof(g_log_dir), "%s/logs", g_config_dir);

    g_initialized = true;
    return 0;
}

void sanctify_shutdown(void) {
    g_initialized = false;
}

const char* sanctify_platform_name(void) {
    return SANCTIFY_PLATFORM_NAME;
}

const char* sanctify_rime_version(void) {
    return get_rime_version_impl();
}

const char* sanctify_build_timestamp(void) {
    return __DATE__ " " __TIME__;
}

int sanctify_is_xinchuang(void) {
#if defined(SANCTIFY_PLATFORM_LOONGARCH) || \
    defined(SANCTIFY_PLATFORM_ARMV8) || \
    defined(__MIPS_XC__) || defined(__ZX_XC__)
    return 1;
#else
    return 0;
#endif
}

char* sanctify_get_exe_dir(void) {
    return strdup(g_exe_dir);
}

char* sanctify_get_config_dir(void) {
    return strdup(g_config_dir);
}

char* sanctify_get_log_path(const char* module_name) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/%s.log", g_log_dir,
             module_name ? module_name : "sanctify");
    return path;
}

char* sanctify_get_temp_dir(void) {
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    static char path[1024];
    snprintf(path, sizeof(path), "%s/sanctify-XXXXXX", tmp);
    char* ret = mkdtemp(path);
    return ret ? strdup(ret) : nullptr;
}

void sanctify_log(int level, const char* module, const char* fmt, ...) {
    (void)level; (void)module; (void)fmt;
    // TODO: 接入真实日志系统 (spdlog / glog)
}

int sanctify_cpu_count(void) {
    return get_nprocs();
}

uint64_t sanctify_total_memory(void) {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.totalram * info.mem_unit;
    }
    return 0;
}

}  // extern "C"
