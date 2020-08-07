//===--- x86GOTAndStubsBuilder.cpp - Generic GOT/Stub creation for x86--*- C++
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
#include "x86GOTAndStubsBuilder.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

using namespace llvm;
using namespace jitlink;
using namespace x86_64_Edges;

bool x86_64_GOTAndStubsBuilder::isGOTEdge(Edge &E) const {
  return E.getKind() == PCRel32GOT || E.getKind() == PCRel32GOTLoad;
}

Symbol &x86_64_GOTAndStubsBuilder::createGOTEntry(Symbol &Target) {
  auto &GOTEntryBlock =
      G.createContentBlock(getGOTSection(), getGOTEntryBlockContent(), 0, 8, 0);
  GOTEntryBlock.addEdge(Pointer64, 0, Target, 0);
  return G.addAnonymousSymbol(GOTEntryBlock, 0, 8, false, false);
}

void x86_64_GOTAndStubsBuilder::fixGOTEdge(Edge &E, Symbol &GOTEntry) {
  assert((E.getKind() == PCRel32GOT || E.getKind() == PCRel32GOTLoad) &&
         "Not a GOT edge?");
  // If this is a PCRel32GOT then change it to an ordinary PCRel32. If it is
  // a PCRel32GOTLoad then leave it as-is for now. We will use the kind to
  // check for GOT optimization opportunities in the
  // optimizeMachO_x86_64_GOTAndStubs pass below.
  if (E.getKind() == PCRel32GOT)
    E.setKind(PCRel32);

  E.setTarget(GOTEntry);
  // Leave the edge addend as-is.
}

bool x86_64_GOTAndStubsBuilder::isExternalBranchEdge(Edge &E) {
  return E.getKind() == Branch32 && !E.getTarget().isDefined();
}

Symbol &x86_64_GOTAndStubsBuilder::createStub(Symbol &Target) {
  auto &StubContentBlock =
      G.createContentBlock(getStubsSection(), getStubBlockContent(), 0, 1, 0);
  // Re-use GOT entries for stub targets.
  auto &GOTEntrySymbol = getGOTEntrySymbol(Target);
  StubContentBlock.addEdge(PCRel32, 2, GOTEntrySymbol, 0);
  return G.addAnonymousSymbol(StubContentBlock, 0, 6, true, false);
}

void x86_64_GOTAndStubsBuilder::fixExternalBranchEdge(Edge &E, Symbol &Stub) {
  assert(E.getKind() == Branch32 && "Not a Branch32 edge?");
  assert(E.getAddend() == 0 && "Branch32 edge has non-zero addend?");

  // Set the edge kind to Branch32ToStub. We will use this to check for stub
  // optimization opportunities in the optimize ELF_x86_64_GOTAndStubs pass
  // below.
  E.setKind(Branch32ToStub);
  E.setTarget(Stub);
}

Section &x86_64_GOTAndStubsBuilder::getGOTSection() {
  if (!GOTSection)
    GOTSection = &G.createSection("$__GOT", sys::Memory::MF_READ);
  return *GOTSection;
}

Section &x86_64_GOTAndStubsBuilder::getStubsSection() {
  if (!StubsSection) {
    auto StubsProt = static_cast<sys::Memory::ProtectionFlags>(
        sys::Memory::MF_READ | sys::Memory::MF_EXEC);
    StubsSection = &G.createSection("$__STUBS", StubsProt);
  }
  return *StubsSection;
}

StringRef x86_64_GOTAndStubsBuilder::getGOTEntryBlockContent() {
  return StringRef(reinterpret_cast<const char *>(NullGOTEntryContent),
                   sizeof(NullGOTEntryContent));
}

StringRef x86_64_GOTAndStubsBuilder::getStubBlockContent() {
  return StringRef(reinterpret_cast<const char *>(StubContent),
                   sizeof(StubContent));
}
const uint8_t x86_64_GOTAndStubsBuilder::NullGOTEntryContent[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t x86_64_GOTAndStubsBuilder::StubContent[6] = {0xFF, 0x25, 0x00,
                                                           0x00, 0x00, 0x00};

Error x86_64_GOTAndStubsBuilder::Optimize_x86_64_GOTAndStubs(
    LinkGraph &G, std::function<StringRef(Edge::Kind)> RelocationName) {
  LLVM_DEBUG(dbgs() << "Optimizing GOT entries and stubs:\n");

  for (auto *B : G.blocks())
    for (auto &E : B->edges())
      if (E.getKind() == PCRel32GOTLoad) {
        assert(E.getOffset() >= 3 && "GOT edge occurs too early in block");

        // Switch the edge kind to PCRel32: Whether we change the edge target
        // or not this will be the desired kind.
        E.setKind(PCRel32);

        // Optimize GOT references.
        auto &GOTBlock = E.getTarget().getBlock();
        assert(GOTBlock.getSize() == G.getPointerSize() &&
               "GOT entry block should be pointer sized");
        assert(GOTBlock.edges_size() == 1 &&
               "GOT entry should only have one outgoing edge");

        auto &GOTTarget = GOTBlock.edges().begin()->getTarget();
        JITTargetAddress EdgeAddr = B->getAddress() + E.getOffset();
        JITTargetAddress TargetAddr = GOTTarget.getAddress();

        // Check that this is a recognized MOV instruction.
        // FIXME: Can we assume this?
        constexpr uint8_t MOVQRIPRel[] = {0x48, 0x8b};
        if (strncmp(B->getContent().data() + E.getOffset() - 3,
                    reinterpret_cast<const char *>(MOVQRIPRel), 2) != 0)
          continue;

        int64_t Displacement = TargetAddr - EdgeAddr + 4;
        if (Displacement >= std::numeric_limits<int32_t>::min() &&
            Displacement <= std::numeric_limits<int32_t>::max()) {
          E.setTarget(GOTTarget);
          auto *BlockData = reinterpret_cast<uint8_t *>(
              const_cast<char *>(B->getContent().data()));
          BlockData[E.getOffset() - 2] = 0x8d;
          LLVM_DEBUG({
            dbgs() << "  Replaced GOT load wih LEA:\n    ";
            printEdge(dbgs(), *B, E, RelocationName(E.getKind()));
            dbgs() << "\n";
          });
        }
      } else if (E.getKind() == Branch32ToStub) {

        // Switch the edge kind to PCRel32: Whether we change the edge target
        // or not this will be the desired kind.
        E.setKind(Branch32);

        auto &StubBlock = E.getTarget().getBlock();
        assert(StubBlock.getSize() ==
                   sizeof(x86_64_GOTAndStubsBuilder::StubContent) &&
               "Stub block should be stub sized");
        assert(StubBlock.edges_size() == 1 &&
               "Stub block should only have one outgoing edge");

        auto &GOTBlock = StubBlock.edges().begin()->getTarget().getBlock();
        assert(GOTBlock.getSize() == G.getPointerSize() &&
               "GOT block should be pointer sized");
        assert(GOTBlock.edges_size() == 1 &&
               "GOT block should only have one outgoing edge");

        auto &GOTTarget = GOTBlock.edges().begin()->getTarget();
        JITTargetAddress EdgeAddr = B->getAddress() + E.getOffset();
        JITTargetAddress TargetAddr = GOTTarget.getAddress();

        int64_t Displacement = TargetAddr - EdgeAddr + 4;
        if (Displacement >= std::numeric_limits<int32_t>::min() &&
            Displacement <= std::numeric_limits<int32_t>::max()) {
          E.setTarget(GOTTarget);
          LLVM_DEBUG({
            dbgs() << "  Replaced stub branch with direct branch:\n    ";
            printEdge(dbgs(), *B, E, RelocationName(E.getKind()));
            dbgs() << "\n";
          });
        }
      }

  return Error::success();
}