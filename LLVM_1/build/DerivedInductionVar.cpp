/* DerivedInductionVar.cpp 
 *
 * This pass detects derived induction variables using ScalarEvolution.
 *
 * Compatible with New Pass Manager
*/

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

class DerivedInductionVar
    : public PassInfoMixin<DerivedInductionVar> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    auto &LI = AM.getResult<LoopAnalysis>(F);
    auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

    for (Loop *L : LI) {
      errs() << "Analyzing loop in function " << F.getName() << ":\n";

      BasicBlock *Header = L->getHeader();
      if (!Header)
          continue;

      for (PHINode &PN : Header->phis()) {
          if (!PN.getType()->isIntegerTy())
            continue;
          const SCEV *S = SE.getSCEV(&PN);

          // Detect affine AddRec expressions
          if (auto *AR = dyn_cast<SCEVAddRecExpr>(S)) {
            const SCEV *Step = AR->getStepRecurrence(SE);
            const SCEV *Start = AR->getStart();

            // Check if it's affine
            if (AR->isAffine()) {
              errs() << "  Derived induction variable: " << PN.getName()
                     << " = {" << *Start << ",+," << *Step << "}<"
                     << L->getHeader()->getName() << ">\n";
            }
          }
      }
    }

    return PreservedAnalyses::all();
  }
};

} // namespace

// Register the pass
llvm::PassPluginLibraryInfo getDerivedInductionVarPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DerivedInductionVar", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "derived-iv") {
                    FPM.addPass(DerivedInductionVar());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDerivedInductionVarPluginInfo();
}
