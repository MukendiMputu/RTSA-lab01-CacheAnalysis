//=============================================================================
// FILE:
//    CacheAnalysisPass.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    New PM:
//      opt -load-pass-plugin=libCacheAnalysisPass.dylib -passes=lru-misses `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <cstdlib>
#include <utility>

#include "../include/AbstractCache.h"
#include "../include/CacheType.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// CacheAnalysisPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

// New PM implementation

struct CacheAnalysisPass : PassInfoMixin<CacheAnalysisPass> {

  // Development Options
  bool PrintAddresses = false;
  bool PrintEdges = false;
  bool PrintEdgesPost = false;
  bool DumpToDot = false;
  bool DumpNodes = false;
  bool LoopUnrolling = false;

  // Assume a 4kB Cache
  // with 16 Sets, associativity of 4 and Cachelines fitting two times the instruction size
  CacheType Cache = CacheType(16, 4, 128);
  StringRef EntryPoint = "main";
  unsigned int EntryAddress;
  unsigned int AddressCounter = 0b100000;
  // assume 8 Bit addressed 64 Bit instructions.
  std::map<unsigned int, Value *> Addr2Value;
  std::map<Value *, unsigned int> Value2Addr;

  AbstractCache AC;
  std::map<Function *, bool> VisitedFunctions;

  unsigned int stringRefToInt(StringRef SR) {
    unsigned int Length = SR.size();
    unsigned int Ret = 0;
    unsigned int Count = 1;
    for (char C : SR) {
      unsigned int Factor = (unsigned int)pow(10, (Length - Count++));
      switch (C) {
      case '0':
        break;
      case '1':
        Ret += Factor;
        break;
      case '2':
        Ret += 2 * Factor;
        break;
      case '3':
        Ret += 3 * Factor;
        break;
      case '4':
        Ret += 4 * Factor;
        break;
      case '5':
        Ret += 5 * Factor;
        break;
      case '6':
        Ret += 6 * Factor;
        break;
      case '7':
        Ret += 7 * Factor;
        break;
      case '8':
        Ret += 8 * Factor;
        break;
      case '9':
        Ret += 9 * Factor;
        break;
      default:
        errs() << "StringRef is not a decimal number";
      };
    }
    return Ret;
  }

  void addressCollector(Module &M) {
    for (Function &F : M) {
      if (F.getName().equals(EntryPoint)) {
        EntryAddress = AddressCounter;
        if (PrintAddresses)
          outs() << "Found main at PseudoAddress: " << EntryAddress << " \n";
      }
      unsigned int InstCounter = 0;
      for (BasicBlock &BB : F) {
        for (Instruction &Inst : BB) {
          AC.addEmptyNode(AddressCounter);
          Addr2Value[AddressCounter] = &Inst;
          Value2Addr[&Inst] = AddressCounter;
          AddressCounter += 1;
          InstCounter++;
        }
      }
    }
  }

  void addressPrinter(Function &F) {
    outs() << "F: " << Value2Addr[&F] << ".\n";
    for (BasicBlock &BB : F) {
      outs() << "-BB: " << Value2Addr[&BB] << "\n";
      for (Instruction &Inst : BB) {
        outs() << "--InstAddress:" << Value2Addr[&Inst] << "\n";
      }
    }
  }

  void initEdges(Function &F) {
    for (BasicBlock &BB : F) {
      // Collect Controll flow in F
      for (auto *Pred : predecessors(&BB)) {
        AC.addEdge(Value2Addr[&Pred->getInstList().back()],
                   Value2Addr[&BB.getInstList().front()]);
        if (PrintEdges)
          outs() << Value2Addr[&Pred->getInstList().back()] << " -> "
                 << Value2Addr[&BB.getInstList().front()] << "\n";
      }
      Instruction *PrevInst = nullptr;
      for (Instruction &Inst : BB) {
        // Collect function Calls in F=main
        if (CallInst *Caller = dyn_cast<CallInst>(&Inst)) {
          Function *Callee = Caller->getCalledFunction();
          if (PrintEdges)
            outs() << "F: " << Callee->getName() << "\n"
                   << "Inst: " << Caller->getName() << "\n";
          if (Callee != NULL) {
            // Add edge on Function Call
            AC.addEdge(Value2Addr[&Inst],
                       Value2Addr[&Callee->getBasicBlockList()
                                       .front()
                                       .getInstList()
                                       .front()]);
            // Add edge on Function return
            AC.addEdge(
                Value2Addr
                    [&Callee->getBasicBlockList().back().getInstList().back()],
                Value2Addr[&Inst]);

            if (PrintEdges) {
              // Printing edge on Function Call
              outs() << Callee->getName() << ": ";
              outs() << Value2Addr[&Inst] << " -> "
                     << Value2Addr[&Callee->getBasicBlockList()
                                        .front()
                                        .getInstList()
                                        .front()]
                     << "\n";
              // Printing edge on Function return
              outs() << Callee->getName() << ": ";
              outs() << Value2Addr[&Callee->getBasicBlockList()
                                        .back()
                                        .getInstList()
                                        .back()]
                     << " -> " << Value2Addr[&Inst] << "\n";
            }

            // Resume CFG construction in called function
            if (VisitedFunctions.find(Callee) == VisitedFunctions.end()) {
              VisitedFunctions[Callee] = true;
              initEdges(*Callee);
            }
            PrevInst = nullptr;
            if (PrintEdges)
              outs() << "Back from " << Callee->getName() << "\n";
          }
        }
        if (PrevInst != nullptr) {
          AC.addEdge(Value2Addr[PrevInst], Value2Addr[&Inst]);
          if (PrintEdges)
            outs() << Value2Addr[PrevInst] << " -> " << Value2Addr[&Inst]
                   << "\n";
        }
        PrevInst = &Inst;
      }
    }
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    FunctionAnalysisManager &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    addressCollector(M);
    Function *EntryFunction;
    for (Function &F : M.getFunctionList()) {
      // Start iterating through CFG from entry point
      if (F.getName().equals(EntryPoint)) {
        EntryFunction = &F;
        initEdges(F);
      }
      if (PrintAddresses)
        addressPrinter(F);
    }
    if(LoopUnrolling)
      AC.unrollLoops();
    AC.runMustAnalysis(EntryAddress);
    if (DumpNodes)
      AC.dumpNodes();
    if (PrintEdgesPost)
      AC.dumpEdges();
    if (DumpToDot)
      AC.dumpDotFile();
    outs() << "MustHits: " << AC.collectHits() << "\n";
    //outs() << "MayMisses: " << AC.collectMisses() << "\n";
    return PreservedAnalyses::all();
  }
};

} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getCacheAnalysisPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CacheAnalysisPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "lru-misses") {
                    MPM.addPass(CacheAnalysisPass());
                    return true; // only looks at CFG
                  }
                  return false; // Analysis pass.
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize CacheAnalysisPass when added to the pass pipeline on
// the command line, i.e. via '-passes=lru-misses'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getCacheAnalysisPassPluginInfo();
}
