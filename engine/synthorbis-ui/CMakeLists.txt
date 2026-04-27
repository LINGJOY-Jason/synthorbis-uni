# SynthOrbis UI — CMakeLists.txt
# Qt 跨平台 UI 层：候选词窗口 + 配置中心 + 托盘图标
# 依赖: synthorbis-engine

cmake_minimum_required(VERSION 3.20)
project(SynthOrbisUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─────────────────────────────────────────────────────────
# Qt 检测（优先 Qt6，降级 Qt5）
# ─────────────────────────────────────────────────────────
set(QT_DEFINITIONS "")
set(QT_LIBRARIES "")
set(QT_INCLUDE_DIRS "")
set(QT_VERSION_STRING "NOT_FOUND")

# 尝试 Qt6
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt6: ${Qt6_DIR}")
    set(QT_VERSION_STRING "Qt6")
    set(QT_LIBRARIES Qt6::Widgets)
    set(QT_DEFINITIONS "SYNTHORBIS_UI_QT6")
else()
    # 降级 Qt5
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    message(STATUS "Found Qt5: ${Qt5_VERSION}")
    set(QT_VERSION_STRING "${Qt5_VERSION}")
    set(QT_LIBRARIES Qt5::Widgets)
    set(QT_DEFINITIONS "SYNTHORBIS_UI_QT5")
    # Qt5 需要手动启用 AUTOMOC 组件
    set(CMAKE_AUTOMOC ON)
endif()

# ─────────────────────────────────────────────────────────
# synthorbis-engine 依赖
# ─────────────────────────────────────────────────────────
set(ENGINE_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../synthorbis-engine")
set(ENGINE_LIBRARIES "")

if(EXISTS "${ENGINE_MODULE_DIR}/CMakeLists.txt")
    add_subdirectory("${ENGINE_MODULE_DIR}" "${CMAKE_BINARY_DIR}/engine-build" EXCLUDE_FROM_ALL)
    set(ENGINE_LIBRARIES synthorbis-engine)
    set(ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../synthorbis-engine/include")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ENGINE_PC QUIET librime>=1.0)
        if(ENGINE_PC_FOUND)
            set(ENGINE_LIBRARIES ${ENGINE_PC_LIBRARIES})
            set(ENGINE_INCLUDE_DIRS ${ENGINE_PC_INCLUDE_DIRS})
        endif()
    endif()
    if(NOT ENGINE_LIBRARIES)
        message(WARNING "synthorbis-engine not found — UI will build in stub mode")
    endif()
endif()

# ─────────────────────────────────────────────────────────
# 头文件目录
# ─────────────────────────────────────────────────────────
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/synthorbis/ui"
    ${ENGINE_INCLUDE_DIRS}
)

# ─────────────────────────────────────────────────────────
# 源文件
# ─────────────────────────────────────────────────────────
set(UI_SOURCES
    src/candidate_window.cc
    src/config_dialog.cc
    src/tray_icon.cc
)

set(UI_HEADERS
    include/synthorbis/ui/candidate_window.h
    include/synthorbis/ui/config_dialog.h
    include/synthorbis/ui/tray_icon.h
    include/synthorbis/ui.h
)

# ─────────────────────────────────────────────────────────
# 静态库
# ─────────────────────────────────────────────────────────
add_library(synthorbis-ui STATIC
    ${UI_SOURCES}
    ${UI_HEADERS}
)

target_compile_definitions(synthorbis-ui PRIVATE
    ${QT_DEFINITIONS}
    $<$<CONFIG:Debug>:QT_DEBUG_PLUGINS=1>
)

target_link_libraries(synthorbis-ui PRIVATE
    ${QT_LIBRARIES}
    ${ENGINE_LIBRARIES}
)

# ─────────────────────────────────────────────────────────
# 可执行测试（简单模式：不依赖 Qt Test）
# ─────────────────────────────────────────────────────────
option(SYNTHORBIS_UI_BUILD_TEST "Build UI test executable" ON)

if(SYNTHORBIS_UI_BUILD_TEST)
    add_executable(test_ui
        test/test_ui.cc
        ${UI_SOURCES}
        ${UI_HEADERS}
    )
    target_compile_definitions(test_ui PRIVATE
        ${QT_DEFINITIONS}
        SYNTHORBIS_UI_TEST
    )
    target_link_libraries(test_ui PRIVATE
        ${QT_LIBRARIES}
        # synthorbis-engine 链接已由 synthorbis-ui 静态库继承
        # （synthorbis-ui 内部使用 engine API）
    )
endif()

# ─────────────────────────────────────────────────────────
# 导出头文件（供外部项目 find_package 使用）
# ─────────────────────────────────────────────────────────
set_target_properties(synthorbis-ui PROPERTIES
    PUBLIC_HEADER "${UI_HEADERS}"
    DEFINE_SYMBOL SYNTHORBIS_UI_EXPORTS
)

install(TARGETS synthorbis-ui
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/synthorbis/ui
)
