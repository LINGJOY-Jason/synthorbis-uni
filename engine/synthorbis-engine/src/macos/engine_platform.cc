// SynthOrbis Engine — macOS 平台实现
//
// macOS 平台特定的引擎功能

#include "synthorbis/common.h"

#ifdef SYNTHORBIS_PLATFORM_MACOS

// macOS 平台初始化
namespace synthorbis {
namespace platform {

void PlatformInit() {
    // macOS 特定初始化
}

void PlatformShutdown() {
    // macOS 特定清理
}

}  // namespace platform
}  // namespace synthorbis

#endif  // SYNTHORBIS_PLATFORM_MACOS
