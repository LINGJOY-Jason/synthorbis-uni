// SynthOrbis Engine — Linux 平台实现
//
// Linux 平台特定的引擎功能

#include "synthorbis/common.h"

#ifdef SYNTHORBIS_PLATFORM_LINUX

// Linux 平台初始化
namespace synthorbis {
namespace platform {

void PlatformInit() {
    // Linux 特定初始化
}

void PlatformShutdown() {
    // Linux 特定清理
}

}  // namespace platform
}  // namespace synthorbis

#endif  // SYNTHORBIS_PLATFORM_LINUX
