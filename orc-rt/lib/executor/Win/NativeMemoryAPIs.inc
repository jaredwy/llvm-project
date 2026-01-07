//===- NativeMemoryAPIs.inc -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generic wrappers for unix-style memory APIs (mmap, mprotect, etc.).
//
//===----------------------------------------------------------------------===//

#include "orc-rt/Error.h"
#include "orc-rt/MemoryFlags.h"

#define NOMINMAX
#include <windows.h>

namespace {

orc_rt::Error GenerateErrorFromGetLastError(const char *Prefix) {
  LPSTR Message = nullptr;

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&Message, 0,
                 nullptr);

  std::string ErrorMsg(Prefix);
  if (Message) {
    ErrorMsg += ": ";
    ErrorMsg += Message;
    LocalFree(Message);
  } else {
    ErrorMsg += ": (Unknown error)";
  }

  return orc_rt::make_error<orc_rt::StringError>(std::move(ErrorMsg));
}

DWORD toNativeProtFlags(orc_rt::MemProt MP) {
  bool IsRead = (MP & orc_rt::MemProt::Read) != orc_rt::MemProt::None;
  bool IsWrite = (MP & orc_rt::MemProt::Write) != orc_rt::MemProt::None;
  bool IsExec = (MP & orc_rt::MemProt::Exec) != orc_rt::MemProt::None;

  if (IsExec) {
    if (IsWrite)
      return PAGE_EXECUTE_READWRITE; // on win write assumes read
    if (IsRead)
      return PAGE_EXECUTE_READ;
    return PAGE_EXECUTE;
  } else {
    if (IsWrite)
      return PAGE_READWRITE;
    if (IsRead)
      return PAGE_READONLY;
    return PAGE_NOACCESS;
  }
}

static void invalidateInstructionCache(void *Addr, size_t Size) {
  FlushInstructionCache(GetCurrentProcess(), Addr, Size);
}

orc_rt::Expected<void *> hostOSMemoryReserve(size_t Size) {
  if (Size == 0)
    return nullptr;

  void *Addr =
      VirtualAlloc(nullptr, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (Addr == nullptr) {
    return GenerateErrorFromGetLastError(
        "VirtualAlloc for memory reserve failed: ");
  }
  return Addr;
}

orc_rt::Error hostOSMemoryRelease(void *Base, size_t Size) {
  if (VirtualFree(Base, 0, MEM_RELEASE) == 0) {
    return GenerateErrorFromGetLastError(
        "VirtualFree for memory release failed: ");
  }
  return orc_rt::Error::success();
}

orc_rt::Error hostOSMemoryProtect(void *Base, size_t Size, orc_rt::MemProt MP) {
  DWORD OldProtect = 0;
  if (VirtualProtect(Base, Size, toNativeProtFlags(MP), &OldProtect) == 0) {
    return GenerateErrorFromGetLastError(
        "VirtualFree for memory release failed: ");
  }

  if ((MP & orc_rt::MemProt::Exec) != orc_rt::MemProt::None)
    invalidateInstructionCache(Base, Size);

  return orc_rt::Error::success();
}

} // anonymous namespace
