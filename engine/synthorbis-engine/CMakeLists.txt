# SynthOrbis Engine — CMake 构建配置
#
# RIME 核心集成层

cmake_minimum_required(VERSION 3.20)
project(SynthOrbisEngine VERSION 1.0.0 LANGUAGES CXX C)

# ─────────────────────────────────────────────────────────
# 编译选项
# ─────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出符号
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# 警告处理
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /WX-
        /permissive-
    )
endif()

# ─────────────────────────────────────────────────────────
# 平台检测
# ─────────────────────────────────────────────────────────

if(WIN32)
    set(PLATFORM_DIR "windows")
elseif(APPLE)
    set(PLATFORM_DIR "macos")
elseif(UNIX AND NOT ANDROID)
    if(EXISTS "/usr/include/HarmonyOS/os_ohos.h")
        set(PLATFORM_DIR "vehicle")
    else()
        set(PLATFORM_DIR "linux")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Building for platform: ${PLATFORM_DIR}")

# ─────────────────────────────────────────────────────────
# 平台抽象层依赖
# ─────────────────────────────────────────────────────────

# 尝试找到 platform 模块（相对本文件目录向上两级）
set(PLATFORM_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../platform")
if(EXISTS "${PLATFORM_MODULE_DIR}/CMakeLists.txt")
    message(STATUS "Found platform module at: ${PLATFORM_MODULE_DIR}")
    add_subdirectory("${PLATFORM_MODULE_DIR}" "${CMAKE_BINARY_DIR}/platform-build")
    set(PLATFORM_LIBRARIES synthorbis-platform)
else()
    message(WARNING "Platform module not found, using Threads only")
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBRARIES Threads::Threads)
endif()

# ─────────────────────────────────────────────────────────
# RIME 核心依赖（可选）
# ─────────────────────────────────────────────────────────

option(SYNTHORBIS_SKIP_RIME "Skip RIME dependency (stub/test mode)" OFF)

# 先查找同级 librime 子模块
set(RIME_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../librime")
set(RIME_LIBRARIES "")
set(RIME_DEFINITIONS "")

if(NOT SYNTHORBIS_SKIP_RIME AND EXISTS "${RIME_MODULE_DIR}/CMakeLists.txt")
    message(STATUS "Found RIME module at: ${RIME_MODULE_DIR}")
    add_subdirectory("${RIME_MODULE_DIR}" "${CMAKE_BINARY_DIR}/rime-build")
    set(RIME_LIBRARIES rime)
    set(RIME_DEFINITIONS HAVE_RIME)
elseif(NOT SYNTHORBIS_SKIP_RIME)
    # 尝试 pkg-config 查找系统 librime
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(RIME QUIET librime>=1.0)
        if(RIME_FOUND)
            message(STATUS "Found system librime via pkg-config")
            set(RIME_LIBRARIES ${RIME_LIBRARIES})
            set(RIME_DEFINITIONS HAVE_RIME)
        else()
            message(WARNING "RIME not found — building without RIME (stub mode)")
        endif()
    else()
        message(WARNING "PkgConfig not found — building without RIME (stub mode)")
    endif()
else()
    message(STATUS "SYNTHORBIS_SKIP_RIME=ON — building in stub mode (no RIME)")
endif()

# ─────────────────────────────────────────────────────────
# 源文件
# ─────────────────────────────────────────────────────────

set(SYNTHORBIS_ENGINE_SOURCES
    src/engine.cc
    src/session.cc
    src/input_adapter.cc
    src/candidates.cc
)

set(PLATFORM_SOURCE_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/${PLATFORM_DIR}/engine_platform.cc")

if(EXISTS "${PLATFORM_SOURCE_FILE}")
    list(APPEND SYNTHORBIS_ENGINE_SOURCES "${PLATFORM_SOURCE_FILE}")
else()
    message(WARNING "Platform source not found: ${PLATFORM_SOURCE_FILE}")
endif()

# 库类型选项：Windows 默认 SHARED，Linux/macOS 默认 STATIC（方便集成无 fPIC 的静态依赖）
if(WIN32)
    option(SYNTHORBIS_BUILD_SHARED "Build synthorbis-engine as shared library" ON)
else()
    option(SYNTHORBIS_BUILD_SHARED "Build synthorbis-engine as shared library" OFF)
endif()

if(SYNTHORBIS_BUILD_SHARED)
    set(SYNTHORBIS_LIB_TYPE SHARED)
else()
    set(SYNTHORBIS_LIB_TYPE STATIC)
endif()

message(STATUS "Building synthorbis-engine as: ${SYNTHORBIS_LIB_TYPE}")

add_library(${PROJECT_NAME} ${SYNTHORBIS_LIB_TYPE}
    ${SYNTHORBIS_ENGINE_SOURCES}
)

# ─────────────────────────────────────────────────────────
# 头文件目录（必须在 add_library 之后）
# ─────────────────────────────────────────────────────────

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# RIME 头文件（私有，仅编译时需要）
if(RIME_DEFINITIONS)
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${RIME_MODULE_DIR}/src"
        "${RIME_MODULE_DIR}/include"
    )
endif()

# ─────────────────────────────────────────────────────────
# 编译定义
# ─────────────────────────────────────────────────────────

target_compile_definitions(${PROJECT_NAME}
    PRIVATE SYNTHORBIS_EXPORTS ${RIME_DEFINITIONS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PLATFORM_LIBRARIES}
    ${RIME_LIBRARIES}
)

# ─────────────────────────────────────────────────────────
# 版本信息（必须在 add_library 之后）
# ─────────────────────────────────────────────────────────

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/synthorbis.h"
)

# ─────────────────────────────────────────────────────────
# 安装
# ─────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ─────────────────────────────────────────────────────────
# 测试
# ─────────────────────────────────────────────────────────

enable_testing()

add_executable(test_engine
    test/test_engine.cc
)

target_link_libraries(test_engine PRIVATE
    ${PROJECT_NAME}
)

add_test(NAME EngineTest COMMAND test_engine)

# ─────────────────────────────────────────────────────────
# 导出
# ─────────────────────────────────────────────────────────

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SynthOrbisEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SynthOrbisEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/SynthOrbisEngine
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SynthOrbisEngineConfig.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/SynthOrbisEngine
)
