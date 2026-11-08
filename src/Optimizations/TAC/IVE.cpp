#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include "Helpers.h"
#include <algorithm>
#include <unordered_set>

bool EliminateInductionVariables(TAC& TAC)
{
    bool changed = false;

    for (auto& TACFunc : TAC)
    {
        TACVarUsesInfo& VarUsesInfo = TACFunc->getVarUsesInfo();
        TACLoopInfo& loopInfo = TACFunc->getLoopInfo();
        TACDefBlocksInfo& DefBlocksInfo = TACFunc->getDefBlocksInfo();
        TACInductionVariableInfo& IndVarInfo = TACFunc->getInductionVariableInfo();

        auto getConstantInt = [&DefBlocksInfo](TACValue& val) -> std::optional<int> {
            if (std::holds_alternative<int>(val))
                return std::get<int>(val);

            TACInstruction* def = getDefiningInst(val, DefBlocksInfo);

            if (def && def->type == TACInstType::ASSIGN && std::holds_alternative<int>(static_cast<TACAssign*>(def)->source))
                return std::get<int>(static_cast<TACAssign*>(def)->source);

            return std::nullopt;
        };

        for (TACLoop* loop : loopInfo.allLoops)
        {
            TACBlock* preheader = loop->Preheader;

            if (!preheader || loop->Latches.empty())
                continue;

            if (IndVarInfo.InductionVariables[loop].size() < 2)
                continue;

            auto getLatchVal = [&loop](TACPhi* phi) -> TACValue {
                for (PhiArgument& arg : phi->args)
                {
                    if (arg.SourceBlock == loop->Latches[0])
                        return arg.Value;
                }

                return phi->variable;
            };

            for (TACInductionVariable& IndVar : IndVarInfo.InductionVariables[loop])
            {
                TACValue baseLatchVal = getLatchVal(IndVar.phi);

                TACInstruction* latchDef = getDefiningInst(baseLatchVal, DefBlocksInfo);

                bool latchIsCopy = latchDef && latchDef->type == TACInstType::ASSIGN && std::holds_alternative<TACVariable>(static_cast<TACAssign*>(latchDef)->source);

                TACValue stepTemp = latchIsCopy ? static_cast<TACAssign*>(latchDef)->source : baseLatchVal;

                TACBranch* exitBranch = nullptr;

                bool usesPhiVal = false;
                bool usesLatchVal = false;

                for (TACBlock* block : loop->Blocks)
                {
                    if (block->Instructions.empty() || block->Instructions.back()->type != TACInstType::BRANCH)
                        continue;

                    TACBranch* branch = static_cast<TACBranch*>(block->Instructions.back().get());
                    auto& cond = branch->cond;

                    if (loop->Blocks.contains(branch->TrueTarget) == loop->Blocks.contains(branch->FalseTarget))
                        continue;

                    usesPhiVal = cond.Left == IndVar.phi->variable || cond.Right == IndVar.phi->variable;
                    usesLatchVal = cond.Left == baseLatchVal || cond.Right == baseLatchVal || cond.Left == stepTemp || cond.Right == stepTemp;

                    if (usesPhiVal || usesLatchVal)
                    {
                        exitBranch = branch;
                        break;
                    }
                }

                if (!exitBranch)
                    continue;

                auto allUsesInternal = [&VarUsesInfo, &IndVar, &exitBranch](TACValue val, bool isTested) {
                    if (!VarUsesInfo.VarUses.contains(val))
                        return true;

                    for (TACInstruction* useInst : VarUsesInfo.VarUses[val])
                    {
                        if (useInst != IndVar.stepInst && useInst != IndVar.phi && !(isTested && useInst == exitBranch))
                            return false;
                    }

                    return true;
                };

                if (!allUsesInternal(IndVar.phi->variable, usesPhiVal) || (baseLatchVal != IndVar.phi->variable && !allUsesInternal(baseLatchVal, usesLatchVal)))
                    continue;

                TACInductionVariable* replacement = nullptr;
                TACValue scale{};

                auto baseInit = getConstantInt(IndVar.initVal);
                auto baseStep = getConstantInt(IndVar.stepVal);

                for (TACInductionVariable& candidate : IndVarInfo.InductionVariables[loop])
                {
                    if (&candidate == &IndVar || candidate.phi == nullptr)
                        continue;

                    auto candInit = getConstantInt(candidate.initVal);
                    auto candStep = getConstantInt(candidate.stepVal);

                    if (baseInit && baseStep && candInit && candStep && *baseStep != 0 && *candStep % *baseStep == 0)
                    {
                        int k = *candStep / *baseStep;

                        if (*candInit == *baseInit * k)
                        {
                            replacement = &candidate;
                            scale = k;
                            break;
                        }
                    }

                    TACInstruction* initDef = getDefiningInst(candidate.initVal, DefBlocksInfo);
                    TACInstruction* stepDef = getDefiningInst(candidate.stepVal, DefBlocksInfo);

                    if (!initDef || !stepDef || initDef->type != TACInstType::BINARYOP || stepDef->type != TACInstType::BINARYOP)
                        continue;

                    TACBinaryOp* initBin = static_cast<TACBinaryOp*>(initDef);
                    TACBinaryOp* stepBin = static_cast<TACBinaryOp*>(stepDef);

                    bool initHasBase = initBin->left == IndVar.initVal || initBin->right == IndVar.initVal;
                    bool stepHasBase = stepBin->left == IndVar.stepVal || stepBin->right == IndVar.stepVal;

                    TACValue initScale = (initBin->left == IndVar.initVal) ? initBin->right : initBin->left;
                    TACValue stepScale = (stepBin->left == IndVar.stepVal) ? stepBin->right : stepBin->left;

                    if (initBin->op == BinaryOp::MUL && stepBin->op == BinaryOp::MUL && initHasBase && stepHasBase && initScale == stepScale)
                    {
                        replacement = &candidate;
                        scale = initScale;
                        break;
                    }
                }

                if (!replacement)
                    continue;

                bool isEqualityTest = exitBranch->cond.Op == BinaryOp::DOUBLE_EQUAL || exitBranch->cond.Op == BinaryOp::NOT_EQUAL;

                if (!isEqualityTest && !(std::holds_alternative<int>(scale) && std::get<int>(scale) > 0))
                    continue;

                bool ivIsLeft = usesPhiVal ? exitBranch->cond.Left == IndVar.phi->variable : (exitBranch->cond.Left == baseLatchVal || exitBranch->cond.Left == stepTemp);

                TACValue* ivOperand = ivIsLeft ? &exitBranch->cond.Left : &exitBranch->cond.Right;
                TACValue* boundOperand = ivIsLeft ? &exitBranch->cond.Right : &exitBranch->cond.Left;

                if (std::any_of(DefBlocksInfo.DefBlocks[*boundOperand].begin(), DefBlocksInfo.DefBlocks[*boundOperand].end(), [loop](TACBlock* defBlock) { return loop->Blocks.contains(defBlock); }))
                    continue;

                TACValue scaledBound = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};

                IREditor<TACTypes>::addInstructionBefore(preheader, preheader->Instructions.back().get(), std::make_unique<TACBinaryOp>(scaledBound, *boundOperand, BinaryOp::MUL, scale));

                *boundOperand = scaledBound;
                *ivOperand = usesPhiVal ? replacement->phi->variable : getLatchVal(replacement->phi);

                for (PhiArgument& arg : IndVar.phi->args)
                {
                    arg.Value = 0;
                }

                IndVar.stepInst->left = 0;
                IndVar.stepInst->right = 0;
                IndVar.phi = nullptr;
                changed = true;
            }
        }
    }

    return changed;
}
