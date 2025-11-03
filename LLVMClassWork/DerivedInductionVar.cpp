/* DerivedInductionVar.cpp 
 *
 * This pass detects derived induction variables using ScalarEvolution.
 *
 * Compatible with New Pass Manager
*/

#include "llvm/IR/IRBuilder.h"
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
      eliminateLoopIVsRecursively(L, SE);
    }

    return PreservedAnalyses::none(); // IR is modified
  }

private:
  void eliminateLoopIVsRecursively(Loop *L, ScalarEvolution &SE) {
    BasicBlock *Header = L->getHeader();
    if (!Header)
      return;

    // Get canonical induction variable for this loop
    PHINode *CanonicalIV = L->getCanonicalInductionVariable();
    if (!CanonicalIV)
      return; // skip if no canonical IV

    for (PHINode &PN : Header->phis()) {
      if (&PN == CanonicalIV)
        continue; // skip canonical IV itself
      if (!PN.getType()->isIntegerTy())
        continue;

      const SCEV *S = SE.getSCEV(&PN);
      if (auto *AR = dyn_cast<SCEVAddRecExpr>(S)) {
        if (!AR->isAffine())
          continue;

        const SCEV *Start = AR->getStart();
        const SCEV *Step = AR->getStepRecurrence(SE);

        // Only handle integer constants
        if (auto *StartC = dyn_cast<SCEVConstant>(Start)) {
          if (auto *StepC = dyn_cast<SCEVConstant>(Step)) {
            IRBuilder<> Builder(&PN);
            APInt StartVal = StartC->getAPInt();
            APInt StepVal = StepC->getAPInt();

            // newVal = Start + Step * canonicalIV
            Value *StepMulIV = Builder.CreateMul(
                Builder.getInt(StepVal),
                CanonicalIV, "step_mul_iv");
            Value *NewVal = Builder.CreateAdd(
                Builder.getInt(StartVal),
                StepMulIV, "derived_iv_repl");

            PN.replaceAllUsesWith(NewVal);
            PN.eraseFromParent();

            errs() << "Replaced derived IV " << PN.getName()
                   << " with expression: " << *NewVal << "\n";
          }
        }
      }
    }

    // Recurse into inner loops
    for (Loop *SubL : L->getSubLoops()) {
      eliminateLoopIVsRecursively(SubL, SE);
    }
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
