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
# 头文件目录
# ─────────────────────────────────────────────────────────

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ─────────────────────────────────────────────────────────
# 平台抽象层依赖
# ─────────────────────────────────────────────────────────

# 尝试找到 platform 模块
set(PLATFORM_MODULE_DIR "${CMAKE_SOURCE_DIR}/../../platform")
if(EXISTS "${PLATFORM_MODULE_DIR}/CMakeLists.txt")
    message(STATUS "Found platform module at: ${PLATFORM_MODULE_DIR}")
    add_subdirectory("${PLATFORM_MODULE_DIR}" "${CMAKE_BINARY_DIR}/platform-build")
    set(PLATFORM_LIBRARIES synthorbis-platform)
else()
    message(WARNING "Platform module not found at: ${PLATFORM_MODULE_DIR}")
    # 创建空的 platform 依赖
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBRARIES Threads::Threads)
endif()

# ─────────────────────────────────────────────────────────
# RIME 核心依赖
# ─────────────────────────────────────────────────────────

set(RIME_MODULE_DIR "${CMAKE_SOURCE_DIR}/librime")
if(EXISTS "${RIME_MODULE_DIR}/CMakeLists.txt")
    message(STATUS "Found RIME module at: ${RIME_MODULE_DIR}")
    add_subdirectory("${RIME_MODULE_DIR}" "${CMAKE_BINARY_DIR}/rime-build")
    set(RIME_LIBRARIES rime)
else()
    message(WARNING "RIME module not found, using find_package")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RIME REQUIRED librime>=1.0)
    set(RIME_LIBRARIES ${RIME_LIBRARIES})
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

set(PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${PLATFORM_DIR}/engine_platform.cc
)

add_library(${PROJECT_NAME} SHARED
    ${SYNTHORBIS_ENGINE_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PLATFORM_LIBRARIES}
    ${RIME_LIBRARIES}
)

# ─────────────────────────────────────────────────────────
# 版本信息
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
