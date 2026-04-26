// =============================================================
// SynthOrbis UNI - 类型系统测试
// =============================================================

#include <cstdio>
#include <cassert>
#include "platform/types.h"

// 测试固定宽度整数
void test_fixed_width_integers(void) {
    printf("[TEST] Fixed-width integers:\n");

    assert(sizeof(i8) == 1);  printf("  i8:  OK (1 byte)\n");
    assert(sizeof(i16) == 2); printf("  i16: OK (2 bytes)\n");
    assert(sizeof(i32) == 4); printf("  i32: OK (4 bytes)\n");
    assert(sizeof(i64) == 8); printf("  i64: OK (8 bytes)\n");

    assert(sizeof(u8) == 1);  printf("  u8:  OK (1 byte)\n");
    assert(sizeof(u16) == 2); printf("  u16: OK (2 bytes)\n");
    assert(sizeof(u32) == 4); printf("  u32: OK (4 bytes)\n");
    assert(sizeof(u64) == 8); printf("  u64: OK (8 bytes)\n");

    printf("[PASS] Fixed-width integers test passed!\n");
}

// 测试指针宽度整数
void test_pointer_width_integers(void) {
    printf("[TEST] Pointer-width integers:\n");
    printf("  isize: %zu bytes\n", sizeof(isize));
    printf("  usize: %zu bytes\n", sizeof(usize));
    printf("[PASS] Pointer-width integers test passed!\n");
}

// 测试 Unicode 类型
void test_unicode_types(void) {
    printf("[TEST] Unicode types:\n");
    printf("  c8 (UTF-8 char):  %zu bytes\n", sizeof(c8));
    printf("  c16 (UTF-16 char): %zu bytes\n", sizeof(c16));
    printf("  c32 (UTF-32 char): %zu bytes\n", sizeof(c32));
    printf("  wc (wchar_t):      %zu bytes\n", sizeof(wc));
    printf("[PASS] Unicode types test passed!\n");
}

// 测试路径分隔符
void test_path_separators(void) {
    printf("[TEST] Path separators:\n");
    printf("  SANCTIFY_PATH_SEP: '%c'\n", SANCTIFY_PATH_SEP);
    printf("  SANCTIFY_PATH_SEP_STR: \"%s\"\n", SANCTIFY_PATH_SEP_STR);

#if defined(_WIN32)
    assert(SANCTIFY_PATH_SEP == '\\');
#else
    assert(SANCTIFY_PATH_SEP == '/');
#endif

    printf("[PASS] Path separators test passed!\n");
}
