#include "orc-rt/ExecutorProcessInfo.h"
#define WIN32_LEAN_AND_MEAN

#include <intrin.h>
#include <windows.h>

namespace orc_rt {

std::vector<std::string_view> ExecutorProcessInfo::detectTargetCPUFeatures() {
  std::vector<std::string_view> Features;

  auto A = [&](bool Present, std::string_view Feature) {
    if (Present)
      Features.push_back(Feature);
  };

  int Regs[4] = {};
  __cpuid(Regs, 0);

  const unsigned MaxLeaf = static_cast<unsigned>(Regs[0]);
  if (MaxLeaf < 1)
    return Features;

  __cpuidex(Regs, 1, 0);

  const unsigned ECX = static_cast<unsigned>(Regs[2]);
  const unsigned EDX = static_cast<unsigned>(Regs[3]);

  A(EDX & (1u << 25), "sse");
  A(EDX & (1u << 26), "sse2");

  A(ECX & (1u << 0), "sse3");
  A(ECX & (1u << 9), "ssse3");
  A(ECX & (1u << 19), "sse4.1");
  A(ECX & (1u << 20), "sse4.2");

  const bool HasXSAVE = ECX & (1u << 26);
  const bool HasOSXSAVE = ECX & (1u << 27);
  const bool HasAVXHardware = ECX & (1u << 28);

  uint64_t XCR0 = 0;
  if (HasXSAVE && HasOSXSAVE)
    XCR0 = _xgetbv(0);

  const bool HasAVXState = (XCR0 & 0x6) == 0x6;
  const bool HasAVX = HasAVXHardware && HasAVXState;

  A(HasAVX, "avx");

  if (MaxLeaf >= 7) {
    __cpuidex(Regs, 7, 0);

    const unsigned EBX = static_cast<unsigned>(Regs[1]);

    A(HasAVX && (EBX & (1u << 5)), "avx2");
  }

  return Features;
}

long ExecutorProcessInfo::getPageSize() noexcept {
  SYSTEM_INFO Info;
  GetSystemInfo(&Info);
  return static_cast<long>(Info.dwPageSize);
}

} // namespace orc_rt
