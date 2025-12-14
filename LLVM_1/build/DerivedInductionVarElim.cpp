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

class DerivedInductionVar : public PassInfoMixin<DerivedInductionVar> {
public:

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
	errs() << "DerivedInductionVar pass running on function: " << F.getName() << "\n";
        auto &LI = AM.getResult<LoopAnalysis>(F);
        auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

        for (Loop *L : LI) {
            analyzeLoop(L, SE);
        }

        // Since we transform IR, nothing is preserved
        return PreservedAnalyses::none();
    }

private:
    void analyzeLoop(Loop *L, ScalarEvolution &SE) {

	errs() << "Analyzing loop with header: " << L->getHeader()->getName() << "\n";

        BasicBlock *Header = L->getHeader();
        if (!Header) return;

	SmallVector<PHINode*,4> toErase;

        for (PHINode &PN : Header->phis()) {
            if (!PN.getType()->isIntegerTy()) continue;

            const SCEV *S = SE.getSCEV(&PN);
            if (auto *AR = dyn_cast<SCEVAddRecExpr>(S)) {
                if (AR->isAffine()) {
                    errs() << "Derived IV detected: " << PN.getName() << "\n";
                    // Attempt elimination
		    // Only eliminate if start is a constant
                    if (auto *StartConst = dyn_cast<SCEVConstant>(AR->getStart())) {
                        toErase.push_back(&PN);
                    }
		    //eliminateIV(PN, AR, SE);
                }
            }
        }
	for (PHINode *PN : toErase) {
        	const SCEV *S = SE.getSCEV(PN);
	        auto *AR = dyn_cast<SCEVAddRecExpr>(S);
        	if (!AR) continue;

        	if (auto *StartConst = dyn_cast<SCEVConstant>(AR->getStart())) {
            		ConstantInt *CI = StartConst->getValue();
	            	PN->replaceAllUsesWith(CI);
	            	errs() << "Eliminating IV: " << PN->getName() << "\n";
        	}
        	PN->eraseFromParent();
    	}

    	// Recurse into inner loops
    	for (Loop *Inner : L->getSubLoops()) {
        	analyzeLoop(Inner, SE);
    	}
    }

    void eliminateIV(PHINode &PN, const SCEVAddRecExpr *AR, ScalarEvolution &SE) {
        // Only handle simple affine step
        const SCEV *Step = AR->getStepRecurrence(SE);
        const SCEV *Start = AR->getStart();

        // For demonstration, replace uses with the start value
        // (more advanced: compute exact replacement using other IVs)
        if (auto *StartConst = dyn_cast<SCEVConstant>(Start)) {
            ConstantInt *CI = StartConst->getValue();
            PN.replaceAllUsesWith(CI);
            errs() << "Eliminating IV: " << PN.getName() << "\n";

            // Remove PHI from the header
            PN.eraseFromParent();
        }
        // Otherwise, skip complex elimination for now
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
                        if (Name == "ind-var-elim") {
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
