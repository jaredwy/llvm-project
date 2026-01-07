//===- NativeDylibAPIs.inc --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generic wrappers for windows dlfcn.h APIs.
//
//===----------------------------------------------------------------------===//
#define WIN32_LEAN_AND_MEAN
#include <optional>
#include <vector>
#include <windows.h>
#include <psapi.h>

#include "../windows/Error.h"
#include "orc-rt/Error.h"

namespace {

using DylibHandle = orc_rt::NativeDylibManager::DylibHandle;
using SymbolLookupResult = orc_rt::NativeDylibManager::SymbolLookupResult;

std::optional<void *> lookupSymbol(HMODULE Module, const std::string &Name) {
  if (auto Addr = GetProcAddress(Module, Name.c_str()))
    return reinterpret_cast<void *>(Addr);

  return std::nullopt;
}

SymbolLookupResult
hostOSLookup(HMODULE Module, const std::vector<std::string> &Names) {
  SymbolLookupResult Result;
  Result.reserve(Names.size());

  for (const auto &Name : Names)
    Result.push_back(lookupSymbol(Module, Name));

  return Result;
}

std::vector<HMODULE> getProcessModules() {
  HANDLE Process = GetCurrentProcess();

  HMODULE Module;
  DWORD BytesNeeded = 0;

  if (!EnumProcessModulesEx(Process, &Module, sizeof(Module), &BytesNeeded,
                            LIST_MODULES_64BIT))
    return {};

  std::vector<HMODULE> Modules(BytesNeeded / sizeof(HMODULE));
  const DWORD BufferSize = static_cast<DWORD>(Modules.size() * sizeof(HMODULE));

  DWORD BytesWritten = 0;
  if (!EnumProcessModulesEx(Process, Modules.data(), BufferSize, &BytesWritten,
                            LIST_MODULES_64BIT) ||
      BytesWritten > BufferSize)
    return {};

  Modules.resize(BytesWritten / sizeof(HMODULE));
  return Modules;
}

orc_rt::Expected<DylibHandle>
hostOSLoadLibrary(const std::string &Path) {
  assert(!Path.empty() && "hostOSLoadLibrary doesn't support empty paths");

  HMODULE Handle = LoadLibraryA(Path.c_str());
  if (!Handle)
    return orc_rt::generateErrorFromGetLastError(
        "error loading \"" + Path + "\"");

  return DylibHandle{DylibHandle::Kind::Library,
                     reinterpret_cast<void *>(Handle)};
}

orc_rt::Error hostOSUnloadLibrary(const DylibHandle &Handle) {
  assert(Handle.K == DylibHandle::Kind::Library &&
         "global dylib handle must not be unloaded");
  assert(Handle.LibraryHandle && "invalid library handle");

  if (!FreeLibrary(reinterpret_cast<HMODULE>(Handle.LibraryHandle)))
    return orc_rt::generateErrorFromGetLastError(
        "error unloading library");

  return orc_rt::Error::success();
}

SymbolLookupResult
hostOSLibraryLookup(const DylibHandle &Handle,
                    const std::vector<std::string> &Names) {
  assert(Handle.K == DylibHandle::Kind::Library &&
         "expected library dylib handle");
  assert(Handle.LibraryHandle && "invalid library handle");

  return hostOSLookup(reinterpret_cast<HMODULE>(Handle.LibraryHandle), Names);
}

SymbolLookupResult
hostOSGlobalLookup(const std::vector<std::string> &Names) {
  auto Modules = getProcessModules();

  SymbolLookupResult Result;
  Result.reserve(Names.size());

  if (Modules.empty()) {
    Result.resize(Names.size(), std::nullopt);
    return Result;
  }

  HMODULE Executable = GetModuleHandleA(nullptr);

  for (const auto &Name : Names) {
    auto Addr = lookupSymbol(Executable, Name);

    if (!Addr) {
      for (auto I = Modules.rbegin(); I != Modules.rend(); ++I) {
        if (*I == Executable)
          continue;

        Addr = lookupSymbol(*I, Name);
        if (Addr)
          break;
      }
    }

    Result.push_back(Addr);
  }

  return Result;
}

} // namespace
