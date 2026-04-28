# synthorbis-ai 测试
# 使用 FetchContent 下载 GTest（避免依赖系统安装）

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# 避免覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# --- ASR 引擎测试 ---
add_executable(test_asr test_asr.cc)
target_link_libraries(test_asr
    PRIVATE synthorbis-ai GTest::gtest_main)

# --- Fbank 特征提取测试 ---
add_executable(test_fbank test_fbank.cc)
target_link_libraries(test_fbank
    PRIVATE synthorbis-ai GTest::gtest_main)

# --- CTC 解码器测试 ---
add_executable(test_ctc_decoder test_ctc_decoder.cc)
target_link_libraries(test_ctc_decoder
    PRIVATE synthorbis-ai GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_asr)
gtest_discover_tests(test_fbank)
gtest_discover_tests(test_ctc_decoder)
