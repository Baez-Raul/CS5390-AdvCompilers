#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

static void dumpMemorySSADot(MemorySSA &MSSA, Function &F) {
  std::error_code EC;
  std::string Filename = F.getName().str() + "_memoryssa.dot";
  raw_fd_ostream OS(Filename, EC, sys::fs::OF_Text);

  if (EC) {
    errs() << "Error opening dot file\n";
    return;
  }

  OS << "digraph MemorySSA {\n";

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *MA = MSSA.getMemoryAccess(&I)) {
        OS << "  \"" << MA << "\" [label=\"";
        MA->print(OS);
        OS << "\"];\n";

        if (auto *MD = dyn_cast<MemoryDef>(MA)) {
          if (auto *Prev = MD->getDefiningAccess()) {
            OS << "  \"" << Prev << "\" -> \"" << MA << "\";\n";
          }
        }
      }
    }
  }

  OS << "}\n";
  errs() << "Wrote " << Filename << "\n";
}

static bool runDSE(MemorySSA &MSSA, Function &F) {
  bool Changed = false;
  SmallVector<Instruction *, 8> ToErase;

  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *MA = MSSA.getMemoryAccess(&I);
      auto *MD = dyn_cast_or_null<MemoryDef>(MA);
      if (!MD) continue;

      MemoryAccess *Next = MD->getNextNode();
      if (!Next) continue;

      // If next access is another def → no intervening use
      if (isa<MemoryDef>(Next)) {
        ToErase.push_back(&I);
      }
    }
  }

  for (auto *I : ToErase) {
    errs() << "DSE removing: " << *I << "\n";
    I->eraseFromParent();
    Changed = true;
  }

  return Changed;
}


struct MemorySSADemoPass : PassInfoMixin<MemorySSADemoPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    auto &MSSAResult = AM.getResult<MemorySSAAnalysis>(F);
    auto &MSSA = MSSAResult.getMSSA();

    errs() << "Analyzing function: " << F.getName() << "\n";

    // Iterate over basic blocks to show all MemoryAccesses
    for (auto &BB : F) {
      errs() << "BasicBlock: " << BB.getName() << "\n";

      // MemoryPhi nodes are found at block entries
      if (auto *Phi = MSSA.getMemoryAccess(&BB)) {
        if (auto *MPhi = dyn_cast<MemoryPhi>(Phi)) {
          errs() << "  MemoryPhi for block " << BB.getName() << ":\n";
          for (unsigned i = 0; i < MPhi->getNumIncomingValues(); ++i) {
            auto *IncomingAcc = MPhi->getIncomingValue(i);
            auto *Pred = MPhi->getIncomingBlock(i);
            errs() << "    from " << Pred->getName() << ": ";
            IncomingAcc->print(errs());
            errs() << "\n";
          }
        }
      }

      // Iterate over instructions for MemoryDef/Use
      for (auto &I : BB) {
        if (auto *MA = MSSA.getMemoryAccess(&I)) {
          errs() << "  ";
          MA->print(errs());
          errs() << "\n";
        }
      }
    }

    dumpMemorySSADot(MSSA, F);
    bool Changed = runDSE(MSSA, F);
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

extern"C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MemorySSADemoPass", "v0.9",
          [](PassBuilder &PB) {
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([] { return MemorySSAAnalysis(); });
                });

            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "memssa-demo") {
                    FPM.addPass(MemorySSADemoPass());
                    return true;
                  }
                  return false;
                });
          }};
}
