cmake_minimum_required(VERSION 3.15)
project(synthorbis-ai LANGUAGES CXX)

# ============================================================================
# 选项
# ============================================================================
option(SYNTHORBIS_AI_SKIP_ONNXRUNTIME "Skip ONNX Runtime (for stub mode)" OFF)
option(SYNTHORBIS_AI_BUILD_TESTS "Build tests" ON)

# ============================================================================
# 版本
# ============================================================================
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

# ============================================================================
# C++ 标准
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# 编译选项
# ============================================================================
if(MSVC)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# 目录结构
# ============================================================================
set(INCLUDE_DIR "include/synthorbis/ai")
set(SRC_DIR "src")

# ============================================================================
# ONNX Runtime
# ============================================================================
if(NOT SYNTHORBIS_AI_SKIP_ONNXRUNTIME)
    if(WIN32)
        # Windows: 使用 vcpkg 或手动安装的 ONNX Runtime
        # 自动检测工作区内 deps/onnxruntime-win-x64-* 目录
        file(GLOB _ORT_CANDIDATE_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/onnxruntime-win-x64-*"
        )
        set(_ORT_INCLUDE_HINTS "")
        set(_ORT_LIB_HINTS "")
        foreach(_d ${_ORT_CANDIDATE_DIRS})
            list(APPEND _ORT_INCLUDE_HINTS "${_d}/include")
            list(APPEND _ORT_LIB_HINTS     "${_d}/lib")
        endforeach()

        find_path(ONNXRUNTIME_INCLUDE_DIRS "onnxruntime_cxx_api.h"
            PATHS
                  ${_ORT_INCLUDE_HINTS}
                  "$ENV{ONNXRUNTIME_ROOT}/include"
                  "C:/onnxruntime/include"
                  "C:/lib/onnxruntime/include"
        )
        find_library(ONNXRUNTIME_LIBRARIES onnxruntime onnxruntime_cxx
            PATHS
                  ${_ORT_LIB_HINTS}
                  "$ENV{ONNXRUNTIME_ROOT}/lib"
                  "C:/onnxruntime/lib"
                  "C:/lib/onnxruntime/lib"
        )
    else()
        find_path(ONNXRUNTIME_INCLUDE_DIRS "onnxruntime_cxx_api.h"
            PATHS "/usr/include" "/usr/local/include" "/opt/onnxruntime/include"
        )
        find_library(ONNXRUNTIME_LIBRARIES onnxruntime onnxruntime_cxx
            PATHS "/usr/lib" "/usr/local/lib" "/opt/onnxruntime/lib"
        )
    endif()
    
    if(ONNXRUNTIME_INCLUDE_DIRS AND ONNXRUNTIME_LIBRARIES)
        set(ONNXRUNTIME_FOUND TRUE)
        message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARIES}")
    else()
        set(ONNXRUNTIME_FOUND FALSE)
        message(WARNING "ONNX Runtime not found, ASR features will be disabled")
    endif()
endif()

# ============================================================================
# 源文件
# ============================================================================
set(AI_SOURCES
    ${SRC_DIR}/asr/asr_engine.cc
    ${SRC_DIR}/asr/fbank.cc
    ${SRC_DIR}/asr/onnx_engine.cc
    ${SRC_DIR}/asr/ctc_decoder.cc
)

set(AI_HEADERS
    ${INCLUDE_DIR}/asr_engine.h
    ${INCLUDE_DIR}/asr_types.h
    ${INCLUDE_DIR}/fbank.h
    ${INCLUDE_DIR}/ctc_decoder.h
    ${INCLUDE_DIR}/onnx_engine.h
)

# ============================================================================
# 库
# ============================================================================
add_library(synthorbis-ai STATIC ${AI_SOURCES} ${AI_HEADERS})

target_include_directories(synthorbis-ai PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(ONNXRUNTIME_FOUND)
    target_include_directories(synthorbis-ai PUBLIC ${ONNXRUNTIME_INCLUDE_DIRS})
    target_link_libraries(synthorbis-ai PRIVATE ${ONNXRUNTIME_LIBRARIES})
    target_compile_definitions(synthorbis-ai PUBLIC SYNTHORBIS_AI_HAS_ONNXRUNTIME=1)
else()
    target_compile_definitions(synthorbis-ai PUBLIC SYNTHORBIS_AI_HAS_ONNXRUNTIME=0)
endif()

# Windows 导出
if(WIN32 AND BUILDING_DLL)
    target_compile_definitions(synthorbis-ai PUBLIC SYNTHORBIS_AI_EXPORTS)
endif()

# ============================================================================
# 测试
# ============================================================================
if(SYNTHORBIS_AI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# ============================================================================
# 安装
# ============================================================================
include(GNUInstallDirs)
install(TARGETS synthorbis-ai
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${INCLUDE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ============================================================================
# 打包
# ============================================================================
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/synthorbis-ai-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/synthorbis-ai-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/synthorbis-ai
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/synthorbis-ai-config-version.cmake
    VERSION ${VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/synthorbis-ai-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/synthorbis-ai-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/synthorbis-ai
)
