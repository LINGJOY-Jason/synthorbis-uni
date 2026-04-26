// =============================================================
// SynthOrbis UNI — platform 模块单元测试
// =============================================================

#include <cstdio>
#include <cstdlib>
#include "platform.h"

// 测试平台检测
void test_platform_detection(void) {
    printf("[TEST] Platform: %s\n", sanctify_platform_name());
    printf("[TEST] RIME Version: %s\n", sanctify_rime_version());
    printf("[TEST] Build Time: %s\n", sanctify_build_timestamp());
    printf("[TEST] CPU Count: %d\n", sanctify_cpu_count());
    printf("[TEST] Total Memory: %llu bytes\n",
           (unsigned long long)sanctify_total_memory());

#if defined(SANCTIFY_PLATFORM_WINDOWS)
    printf("[TEST] Platform Type: Windows\n");
#elif defined(SANCTIFY_PLATFORM_MACOS)
    printf("[TEST] Platform Type: macOS\n");
#elif defined(SANCTIFY_PLATFORM_LINUX)
    printf("[TEST] Platform Type: Linux\n");
    #if defined(SANCTIFY_PLATFORM_LOONGARCH)
    printf("[TEST] Architecture: LoongArch64\n");
    #elif defined(SANCTIFY_PLATFORM_ARMV8)
    printf("[TEST] Architecture: ARM64\n");
    #else
    printf("[TEST] Architecture: x86_64\n");
    #endif
#elif defined(SANCTIFY_PLATFORM_HARMONY)
    printf("[TEST] Platform Type: HarmonyOS\n");
#endif
}

// 测试路径函数
void test_paths(void) {
    sanctify_init();

    char* exe_dir = sanctify_get_exe_dir();
    char* config_dir = sanctify_get_config_dir();
    char* log_path = sanctify_get_log_path("test");
    char* temp_dir = sanctify_get_temp_dir();

    printf("[TEST] Exe Dir: %s\n", exe_dir ? exe_dir : "(null)");
    printf("[TEST] Config Dir: %s\n", config_dir ? config_dir : "(null)");
    printf("[TEST] Log Path: %s\n", log_path ? log_path : "(null)");
    printf("[TEST] Temp Dir: %s\n", temp_dir ? temp_dir : "(null)");

    free(exe_dir);
    free(config_dir);
    free(log_path);
    free(temp_dir);
}

// 测试类型定义
void test_types(void) {
    printf("[TEST] Type sizes:\n");
    printf("  i8:  %zu bytes\n", sizeof(i8));
    printf("  i16: %zu bytes\n", sizeof(i16));
    printf("  i32: %zu bytes\n", sizeof(i32));
    printf("  i64: %zu bytes\n", sizeof(i64));
    printf("  u8:  %zu bytes\n", sizeof(u8));
    printf("  u16: %zu bytes\n", sizeof(u16));
    printf("  u32: %zu bytes\n", sizeof(u32));
    printf("  u64: %zu bytes\n", sizeof(u64));
    printf("  isize: %zu bytes\n", sizeof(isize));
    printf("  usize: %zu bytes\n", sizeof(usize));
    printf("  c8:  %zu bytes\n", sizeof(c8));
    printf("  c16: %zu bytes\n", sizeof(c16));
    printf("  c32: %zu bytes\n", sizeof(c32));
    printf("  wc:  %zu bytes\n", sizeof(wc));

    // 验证断言
    int passed = 1;
    if (sizeof(i8) != 1) { printf("FAIL: i8 size\n"); passed = 0; }
    if (sizeof(i16) != 2) { printf("FAIL: i16 size\n"); passed = 0; }
    if (sizeof(i32) != 4) { printf("FAIL: i32 size\n"); passed = 0; }
    if (sizeof(i64) != 8) { printf("FAIL: i64 size\n"); passed = 0; }
    if (passed) {
        printf("[PASS] All type size tests passed!\n");
    }
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║   SynthOrbis UNI — Platform Test Suite    ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");

    printf("--- Platform Detection ---\n");
    test_platform_detection();
    printf("\n");

    printf("--- Path Functions ---\n");
    test_paths();
    printf("\n");

    printf("--- Type Definitions ---\n");
    test_types();
    printf("\n");

    sanctify_shutdown();

    printf("╔═══════════════════════════════════════════╗\n");
    printf("║            All Tests Complete!           ║\n");
    printf("╚═══════════════════════════════════════════╝\n");

    return 0;
}
