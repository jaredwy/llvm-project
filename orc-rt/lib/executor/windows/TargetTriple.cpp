//===- TargetTriple.cpp - Windows target triple detection -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "orc-rt/ExecutorProcessInfo.h"

namespace orc_rt {
    //TODO: the dylib stuff only supports 64 for now, so we just hardcode.
    std::string ExecutorProcessInfo::detectTargetTriple() noexcept {
    #if defined(_M_X64)
      return "x86_64-pc-windows-msvc";
    #elif defined(_M_ARM64)
      return "aarch64-pc-windows-msvc";
    #else
    #error "Unsupported Windows architecture"
    #endif
    }

} // namespace orc_rt
