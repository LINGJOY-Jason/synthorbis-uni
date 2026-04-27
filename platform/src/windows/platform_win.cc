// =============================================================
// SynthOrbis UNI - Windows 平台实现
// platform/src/windows/platform_win.cc
// =============================================================

#include "platform.h"
#include "platform/types.h"
#include <windows.h>
#include <shlobj.h>
#include <cstdlib>
#include <cstring>

static char g_exe_dir[1024] = {0};
static char g_config_dir[1024] = {0};
static char g_log_dir[1024] = {0};
static bool g_initialized = false;

extern "C" {

int sanctify_init(void) {
    if (g_initialized) return 0;

    // 获取可执行文件目录
    wchar_t exe_w[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, exe_w, MAX_PATH) > 0) {
        wchar_t dir_w[MAX_PATH] = {0};
        wcsncpy(dir_w, exe_w, MAX_PATH);
        wchar_t* last = wcsrchr(dir_w, L'\\');
        if (last) *last = L'\0';
        WideCharToMultiByte(CP_UTF8, 0, dir_w, -1,
                            g_exe_dir, sizeof(g_exe_dir), nullptr, nullptr);
    }

    // 获取用户配置目录
    wchar_t appdata_w[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA,
                                    nullptr, 0, appdata_w))) {
        snprintf(g_config_dir, sizeof(g_config_dir), "%ws\\sanctify",
                 appdata_w);
        snprintf(g_log_dir, sizeof(g_log_dir), "%ws\\logs",
                 appdata_w);
        CreateDirectoryA(g_config_dir, nullptr);
        CreateDirectoryA(g_log_dir, nullptr);
    }

    g_initialized = true;
    return 0;
}

void sanctify_shutdown(void) {
    g_initialized = false;
}

const char* sanctify_platform_name(void) {
    return "Windows";
}

const char* sanctify_rime_version(void) {
    return "1.16.1";  // 存根
}

const char* sanctify_build_timestamp(void) {
    return __DATE__ " " __TIME__;
}

int sanctify_is_xinchuang(void) {
    return 0;  // Windows 一般不是信创平台
}

char* sanctify_get_exe_dir(void) {
    return _strdup(g_exe_dir);
}

char* sanctify_get_config_dir(void) {
    return _strdup(g_config_dir);
}

char* sanctify_get_log_path(const char* module_name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s\\%s.log",
             g_log_dir, module_name ? module_name : "sanctify");
    return _strdup(path);
}

char* sanctify_get_temp_dir(void) {
    wchar_t tmp_w[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, tmp_w);
    static char tmp[1024];
    WideCharToMultiByte(CP_UTF8, 0, tmp_w, -1, tmp, sizeof(tmp), nullptr, nullptr);
    return _strdup(tmp);
}

void sanctify_log(int level, const char* module, const char* fmt, ...) {
    SANCTIFY_UNUSED(level); SANCTIFY_UNUSED(module); SANCTIFY_UNUSED(fmt);
}

int sanctify_cpu_count(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

uint64_t sanctify_total_memory(void) {
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        return ms.ullTotalPhys;
    }
    return 0;
}

}  // extern "C"
