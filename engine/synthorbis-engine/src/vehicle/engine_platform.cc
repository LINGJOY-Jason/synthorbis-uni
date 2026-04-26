// SynthOrbis Engine — 鸿蒙车机平台实现
//
// HarmonyOS 车机平台特定的引擎功能

#include "synthorbis/common.h"

#ifdef SYNTHORBIS_PLATFORM_VEHICLE

// 鸿蒙车机平台初始化
namespace synthorbis {
namespace platform {

void PlatformInit() {
    // 鸿蒙车机特定初始化
}

void PlatformShutdown() {
    // 鸿蒙车机特定清理
}

}  // namespace platform
}  // namespace synthorbis

#endif  // SYNTHORBIS_PLATFORM_VEHICLE
