// SynthOrbis Engine — Windows 平台实现
//
// Windows 平台特定的引擎功能

#include "synthorbis/common.h"

#ifdef SYNTHORBIS_PLATFORM_WINDOWS

#include <windows.h>

// Windows 平台初始化
namespace synthorbis {
namespace platform {

void PlatformInit() {
    // 初始化 COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

void PlatformShutdown() {
    CoUninitialize();
}

}  // namespace platform
}  // namespace synthorbis

#endif  // SYNTHORBIS_PLATFORM_WINDOWS
