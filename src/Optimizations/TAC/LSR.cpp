#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>

struct MultiplicationToReduce
{
    TACBlock* block;
    TACBinaryOp* binary;
    TACInductionVariable* basicIV;
    TACValue invariantVal;
};

bool ReduceLoopMultiplications(TAC& TAC)
{
    bool changed = false;

    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& loopInfo = TACFunc->getLoopInfo();
        TACDefBlocksInfo& DefBlocksInfo = TACFunc->getDefBlocksInfo();
        TACInductionVariableInfo& IndVarInfo = TACFunc->getInductionVariableInfo();
        TACLivenessInfo& LivenessInfo = TACFunc->getLivenessInfo();

        for (TACLoop* loop : loopInfo.allLoops)
        {
            auto& basicIVs = IndVarInfo.InductionVariables[loop];

            if (basicIVs.empty())
                continue;

            TACBlock* preheader = loop->Preheader;

            if (!preheader || loop->Latches.empty())
                continue;

            TACBlock* latch = loop->Latches[0];

            int loopPeakPressure = 0;

            for (TACBlock* block : loop->Blocks)
            {
                loopPeakPressure = std::max(loopPeakPressure, LivenessInfo.BlockLiveness[block].MaxPressure);
            }

            int allocatableRegs = gCompilerOptions[Phase::FPO].enabled ? 15 : 14;

            if (allocatableRegs - loopPeakPressure <= 0)
                continue;

            std::vector<MultiplicationToReduce> mulsToReduce;

            auto isInvariant = [&](TACValue val) {
                if (std::holds_alternative<int>(val))
                    return true;

                auto& defBlocks = DefBlocksInfo.DefBlocks[val];

                if (defBlocks.empty())
                    return true;

                return !loop->Blocks.contains(*defBlocks.begin());
            };

            for (TACBlock* block : loop->Blocks)
            {
                for (auto& inst : block->Instructions)
                {
                    if (inst->type != TACInstType::BINARYOP)
                        continue;

                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    if (binary->op != BinaryOp::MUL)
                        continue;

                    auto leftIVIt = std::find_if(basicIVs.begin(), basicIVs.end(), [&binary](TACInductionVariable& IV) { return IV.phi->variable == binary->left; });
                    bool isLeftIV = (leftIVIt != basicIVs.end());

                    auto rightIVIt = std::find_if(basicIVs.begin(), basicIVs.end(), [&binary](TACInductionVariable& IV) { return IV.phi->variable == binary->right; });
                    bool isRightIV = (rightIVIt != basicIVs.end());

                    if (!((isLeftIV && isInvariant(binary->right)) || (isRightIV && isInvariant(binary->left))))
                        continue;

                    TACValue invariantVal = isLeftIV ? binary->right : binary->left;

                    if (std::holds_alternative<int>(invariantVal))
                        continue;

                    TACInductionVariable* basicIV = isLeftIV ? &(*leftIVIt) : &(*rightIVIt);

                    mulsToReduce.push_back({block, binary, basicIV, invariantVal});
                }
            }

            if (mulsToReduce.empty())
                continue;

            int reductionsAddedThisLoop = 0;

            std::map<std::pair<TACInductionVariable*, TACValue>, TACValue> existingReductions;

            std::unordered_set<TACInstruction*> InstructionsToErase;

            for (auto& mul : mulsToReduce)
            {
                auto key = std::make_pair(mul.basicIV, mul.invariantVal);

                TACValue tPhi;

                if (existingReductions.find(key) != existingReductions.end())
                {
                    tPhi = existingReductions[key];
                }

                else
                {
                    if (reductionsAddedThisLoop >= allocatableRegs - loopPeakPressure)
                        continue;

                    TACValue tInit = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};
                    IREditor<TACTypes>::addInstructionBefore(preheader, preheader->Instructions.back().get(), std::make_unique<TACBinaryOp>(tInit, mul.basicIV->initVal, BinaryOp::MUL, mul.invariantVal));

                    TACValue tStep = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};
                    IREditor<TACTypes>::addInstructionBefore(preheader, preheader->Instructions.back().get(), std::make_unique<TACBinaryOp>(tStep, mul.basicIV->stepVal, BinaryOp::MUL, mul.invariantVal));

                    tPhi = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};
                    auto phiInst = std::make_unique<TACPhi>(tPhi);
                    phiInst->args.push_back(PhiArgument{preheader, tInit});

                    TACValue tNext = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};
                    IREditor<TACTypes>::addInstructionBefore(latch, latch->Instructions.back().get(), std::make_unique<TACBinaryOp>(tNext, tPhi, BinaryOp::PLUS, tStep));

                    phiInst->args.push_back(PhiArgument{latch, tNext});
                    IREditor<TACTypes>::addInstructionBefore(loop->Header, loop->Header->Instructions.front().get(), std::move(phiInst));

                    existingReductions[key] = tPhi;
                    reductionsAddedThisLoop++;
                }

                for (auto& fblock : TACFunc->Blocks)
                {
                    for (auto& Inst : fblock->Instructions)
                    {
                        for (TACValue* use : GetTACInstOperands(Inst.get()).Uses)
                        {
                            if (*use == mul.binary->dest)
                                *use = tPhi;
                        }
                    }
                }

                InstructionsToErase.insert(mul.binary);
                changed = true;
            }

            if (!InstructionsToErase.empty())
            {
                for (TACBlock* block : loop->Blocks)
                {
                    block->Instructions.erase(std::remove_if(block->Instructions.begin(), block->Instructions.end(), [&InstructionsToErase](auto& instPtr) { return InstructionsToErase.contains(instPtr.get()); }), block->Instructions.end());
                }
            }
        }
    }

    return changed;
}
