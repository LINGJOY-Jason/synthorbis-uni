# =============================================================
# SynthOrbis UNI — platform 模块 CMakeLists.txt
# 跨平台抽象层构建配置
# =============================================================

cmake_minimum_required(VERSION 3.20)
project(synthorbis-platform
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "SynthOrbis UNI Cross-Platform Abstraction Layer"
)

# ── C++ 标准 ────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── 编译选项 ────────────────────────────────────────────────
if(MSVC)
  add_compile_options(/W4 /utf-8 /permissive-)
  add_compile_options(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  add_compile_options(/bigobj)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_options(-Wno-unused-parameter)
  add_compile_options(-fPIC)
  add_compile_options(-fvisibility=hidden)
endif()

# ── 平台检测宏（由 CMake 传入）──────────────────────────────
set(SANCTIFY_PLATFORM_FLAG "-DSANCTIFY_LINUX")

if(WIN32)
  set(SANCTIFY_PLATFORM_FLAG "-DSANCTIFY_WINDOWS")
elseif(APPLE)
  set(SANCTIFY_PLATFORM_FLAG "-DSANCTIFY_MACOS")
elseif(SANCTIFY_HARMONY)
  set(SANCTIFY_PLATFORM_FLAG "-DSANCTIFY_HARMONYOS")
endif()

# ── 源码目录 ────────────────────────────────────────────────
set(PLATFORM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PLATFORM_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ── 头文件列表 ─────────────────────────────────────────────
set(PLATFORM_HEADERS
    ${PLATFORM_INCLUDE_DIR}/platform.h
    ${PLATFORM_INCLUDE_DIR}/platform/macros.h
    ${PLATFORM_INCLUDE_DIR}/platform/types.h
    ${PLATFORM_INCLUDE_DIR}/platform/compiler.h
    ${PLATFORM_INCLUDE_DIR}/platform/panic.h
    ${PLATFORM_INCLUDE_DIR}/platform/audio.h
    ${PLATFORM_INCLUDE_DIR}/platform/context.h
    ${PLATFORM_INCLUDE_DIR}/platform/config.h
    ${PLATFORM_INCLUDE_DIR}/platform/thread.h
)

# ── 平台实现源文件（按平台选择）─────────────────────────────
if(WIN32)
  set(PLATFORM_SRC
      ${PLATFORM_SRC_DIR}/windows/platform_win.cc
      ${PLATFORM_SRC_DIR}/windows/audio_wasapi.cc
      ${PLATFORM_SRC_DIR}/windows/config_registry.cc
  )
elseif(APPLE)
  set(PLATFORM_SRC
      ${PLATFORM_SRC_DIR}/macos/platform_apple.cc
      ${PLATFORM_SRC_DIR}/macos/audio_coreaudio.cc
      ${PLATFORM_SRC_DIR}/macos/config_userdefaults.cc
  )
elseif(SANCTIFY_HARMONY)
  # HarmonyOS 车机端实现
  set(PLATFORM_SRC
      ${PLATFORM_SRC_DIR}/vehicle/platform_ohos.cc
      ${PLATFORM_SRC_DIR}/vehicle/audio_ohos.cc
      ${PLATFORM_SRC_DIR}/vehicle/config_preferences.cc
  )
else()
  # Linux 默认实现（含信创平台）
  set(PLATFORM_SRC
      ${PLATFORM_SRC_DIR}/linux/platform_linux.cc
      ${PLATFORM_SRC_DIR}/linux/audio_alsa.cc
      ${PLATFORM_SRC_DIR}/linux/config_xdg.cc
  )
endif()

# ── 头文件搜索路径 ─────────────────────────────────────────
include_directories(SYSTEM ${PLATFORM_INCLUDE_DIR})

# ── 依赖库 ─────────────────────────────────────────────────
set(PLATFORM_LIBS "")

if(WIN32)
  list(APPEND PLATFORM_LIBS winmm imm32 ole32)
elseif(APPLE)
  list(APPEND PLATFORM_LIBS "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
else()
  find_library(ASOUND_LIB asound)
  if(ASOUND_LIB)
    list(APPEND PLATFORM_LIBS ${ASOUND_LIB})
    add_definitions(-DSANCTIFY_HAVE_ALSA=1)
  endif()
endif()

# ── librime 依赖（可选）─────────────────────────────────────
if(DEFINED RIME_DIR)
  include_directories(${RIME_DIR}/include)
endif()

# ── 编译动态库 ──────────────────────────────────────────────
add_library(synthorbis-platform SHARED
    ${PLATFORM_HEADERS}
    ${PLATFORM_SRC}
)

target_compile_definitions(synthorbis-platform PRIVATE
    ${SANCTIFY_PLATFORM_FLAG}
    SANCTIFY_EXPORTS
)

target_link_libraries(synthorbis-platform ${PLATFORM_LIBS})

set_target_properties(synthorbis-platform PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "sanctify-platform"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ── 安装规则 ───────────────────────────────────────────────
install(TARGETS synthorbis-platform
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${PLATFORM_INCLUDE_DIR}/
    DESTINATION include/sanctify
    FILES_MATCHING PATTERN "*.h"
)

# ── 单元测试 ───────────────────────────────────────────────
enable_testing()
add_executable(test-platform
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_platform.cc
)
target_link_libraries(test-platform PRIVATE synthorbis-platform)
add_test(NAME PlatformTest COMMAND test-platform)

# ── 导出 ───────────────────────────────────────────────────
set(SANCTIFY_INCLUDE_DIRS ${PLATFORM_INCLUDE_DIR} PARENT_SCOPE)
set(SANCTIFY_LIBRARY synthorbis-platform PARENT_SCOPE)
