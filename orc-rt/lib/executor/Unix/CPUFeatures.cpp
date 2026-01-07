#include "orc-rt/ExecutorProcessInfo.h"
#include <unistd.c>

namespace orc_rt {
    long ExecutorProcessInfo::getPageSize() noexcept {
      return sysconf(_SC_PAGESIZE);
    }
}
