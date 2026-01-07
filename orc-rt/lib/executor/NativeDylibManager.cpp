//===- NativeDylibManager.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// NativeDylibManager and related APIs.
//
//===----------------------------------------------------------------------===//

#include "orc-rt/NativeDylibManager.h"
#include "orc-rt/Session.h"

#include <sstream> // For NativeDylibAPIs.inc.

#if defined(__APPLE__) || defined(__linux__)
#include "Unix/NativeDylibAPIs.inc"
#elif defined(_WIN32) && defined(_M_X64)
#include "Win/NativeDylibAPIs.inc"
#else
#error "Target OS dylib APIs unsupported"
#endif

namespace orc_rt {

Expected<std::unique_ptr<NativeDylibManager>>
NativeDylibManager::Create(Session &S, SimpleSymbolTable &ST,
                           const char *InstanceName,
                           SimpleSymbolTable::MutatorFn AddInterface) {

  std::unique_ptr<NativeDylibManager> Instance(new NativeDylibManager(S));

  SimpleSymbolTable NDMST;
  if (auto Err = AddInterface(NDMST))
    return Err;
  std::pair<const char *, const void *> InstanceSym[] = {
      {InstanceName, static_cast<const void *>(Instance.get())}};
  if (auto Err = NDMST.addUnique(InstanceSym))
    return std::move(Err);

  if (auto Err = ST.addUnique(NDMST))
    return std::move(Err);

  return std::move(Instance);
}

void NativeDylibManager::load(OnLoadCompleteFn &&OnComplete,
                              std::string Path) {
  // Empty path -> global lookup handle; it is not associated with a loaded
  // library and must not be unloaded.
  if (Path.empty()) {
    static DylibHandle GlobalHandle{DylibHandle::Kind::Global};
    return OnComplete(static_cast<void *>(&GlobalHandle));
  }

  auto H = hostOSLoadLibrary(Path);
  if (!H)
    return OnComplete(H.takeError());

  auto Handle = std::make_unique<DylibHandle>(std::move(*H));

  DylibHandle *Dylib = Handle.get();
  assert(Dylib && "failed to create dylib handle");
  if (!Dylib)
    return OnComplete(
        make_error<StringError>("failed to create dylib handle"));

  // Capture S by reference, rather than this, so that the callback remains
  // valid even if the NativeDylibManager is destroyed prior to shutdown.
  S.addOnShutdown(
      [&S = this->S, Handle = std::move(Handle)]() mutable {
        DylibHandle *Dylib = Handle.get();
        assert(Dylib && "dylib handle unexpectedly null");
        if (!Dylib)
          return;

        assert(Dylib->K == DylibHandle::Kind::Library &&
               "global dylib handle must not be unloaded");

        if (auto Err = hostOSUnloadLibrary(*Dylib))
          S.reportError(std::move(Err));
      });

  OnComplete(static_cast<void *>(Dylib));
}

void NativeDylibManager::lookup(OnLookupCompleteFn &&OnLookupComplete,
                                void *Handle, SymbolLookupSet Symbols) {
  DylibHandle *Dylib = static_cast<DylibHandle *>(Handle);

  assert(Dylib && "invalid dylib handle");
  if (!Dylib)
    return;

  std::vector<std::string> Names;
  Names.reserve(Symbols.size());
  for (auto &S : Symbols)
    Names.push_back(std::move(S.first));

  auto Addrs =
      Dylib->K == DylibHandle::Kind::Global
          ? hostOSGlobalLookup(Names)
          : hostOSLibraryLookup(*Dylib, Names);

  for (size_t I = 0, E = Symbols.size(); I != E; ++I)
    if (!Addrs[I] && Symbols[I].second == WeaklyReferencedSymbol)
      Addrs[I] = nullptr;

  OnLookupComplete(std::move(Addrs));
}

void NativeDylibManager::onDetach(Service::OnCompleteFn OnComplete,
                                  bool ShutdownRequested) {
  // Detach is a noop for now. If/when we add bloom-filter support this will be
  // a good time to update filters.
  OnComplete();
}

void NativeDylibManager::onShutdown(Service::OnCompleteFn OnComplete) {
  // Unloads happen via Session shutdown callbacks registered in load().
  OnComplete();
}

} // namespace orc_rt
