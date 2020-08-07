//===--- x86GOTAndStubsBuilder.h - Generic GOT/Stub creation for x86--*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A base for simple GOT and stub creation for x86
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_EXECUTIONENGINE_JITLINK_x86GOTANDSTUBSBUILDER_H
#define LLVM_LIB_EXECUTIONENGINE_JITLINK_x86GOTANDSTUBSBUILDER_H

#include "BasicGOTAndStubsBuilder.h"

#define DEBUG_TYPE "jitlink"

namespace llvm {
namespace jitlink {

namespace x86_64_Edges {
enum x86RelocationKind : Edge::Kind {
  Branch32 = Edge::FirstRelocation,
  Branch32ToStub,
  Pointer32,
  Pointer64,
  Pointer64Anon,
  PCRel32,
  PCRel32Minus1,
  PCRel32Minus2,
  PCRel32Minus4,
  PCRel32Anon,
  PCRel32Minus1Anon,
  PCRel32Minus2Anon,
  PCRel32Minus4Anon,
  PCRel32GOTLoad,
  PCRel32GOT,
  PCRel32TLV,
  Delta32,
  Delta64,
  NegDelta32,
  NegDelta64,
};
} // namespace x86_64_Edges

class x86_64_GOTAndStubsBuilder
    : public BasicGOTAndStubsBuilder<x86_64_GOTAndStubsBuilder> {
public:
  static const uint8_t NullGOTEntryContent[8];
  static const uint8_t StubContent[6];
  static Error Optimize_x86_64_GOTAndStubs(
      LinkGraph &G, std::function<StringRef(Edge::Kind)> RelocationName);
  x86_64_GOTAndStubsBuilder(LinkGraph &G)
      : BasicGOTAndStubsBuilder<x86_64_GOTAndStubsBuilder>(G) {}

public:
  bool isGOTEdge(Edge &E) const;
  Symbol &createGOTEntry(Symbol &Target);
  void fixGOTEdge(Edge &E, Symbol &GOTEntry);
  bool isExternalBranchEdge(Edge &E);

  Symbol &createStub(Symbol &Target);

  void fixExternalBranchEdge(Edge &E, Symbol &Stub);

private:
  Section &getGOTSection();

  Section &getStubsSection();

  StringRef getGOTEntryBlockContent();

  StringRef getStubBlockContent();

  Section *GOTSection = nullptr;
  Section *StubsSection = nullptr;
};

} // end namespace jitlink
} // end namespace llvm

#endif // LLVM_LIB_EXECUTIONENGINE_JITLINK_x86GOTANDSTUBSBUILDER_H