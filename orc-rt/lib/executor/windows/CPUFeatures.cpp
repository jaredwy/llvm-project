#include "orc-rt/ExecutorProcessInfo.h"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

namespace orc_rt {
    long ExecutorProcessInfo::getPageSize() noexcept {
      SYSTEM_INFO Info;
      GetSystemInfo(&Info);
      return static_cast<long>(Info.dwPageSize);
    }
} // namespace orc_rt
