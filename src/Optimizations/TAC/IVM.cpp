#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <print>

bool MergeInductionVariables(TAC& TAC)
{
    bool changed = false;

    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TACFunc->getLoopInfo();
        TACInductionVariableInfo& IndVarInfo = TACFunc->getInductionVariableInfo();
        TACDefBlocksInfo& DefBlocksInfo = TACFunc->getDefBlocksInfo();

        auto getRootValue = [&DefBlocksInfo](TACValue& val) -> TACValue {
            TACValue curr = val;

            while (true)
            {
                if (DefBlocksInfo.DefBlocks[curr].empty())
                    break;

                TACBlock* defBlock = *DefBlocksInfo.DefBlocks[curr].begin();
                TACInstruction* defInst = nullptr;

                for (auto& blockInst : defBlock->Instructions)
                {
                    TACInstOperands operands = GetTACInstOperands(blockInst.get());

                    if (operands.Def != nullptr && *operands.Def == curr)
                    {
                        defInst = blockInst.get();
                        break;
                    }
                }

                if (!defInst)
                    break;

                TACInstOperands operands = GetTACInstOperands(defInst);

                if (defInst->type == TACInstType::ASSIGN && operands.Uses.size() == 1)
                    curr = **operands.Uses.begin();
                else
                    break;
            }

            return curr;
        };

        for (TACLoop* loop : LoopInfo.allLoops)
        {
            if (IndVarInfo.InductionVariables[loop].empty())
                continue;

            std::vector<TACInductionVariable>& IVList = IndVarInfo.InductionVariables[loop];
            std::vector<std::vector<TACInductionVariable>> groups;

            for (auto& IV : IVList)
            {
                bool matchFound = false;

                for (auto& group : groups)
                {
                    auto& representative = group[0];

                    if (getRootValue(IV.initVal) == getRootValue(representative.initVal) && IV.stepInst->op == representative.stepInst->op && getRootValue(IV.stepVal) == getRootValue(representative.stepVal))
                    {
                        group.push_back(IV);
                        matchFound = true;
                        break;
                    }
                }

                if (!matchFound)
                    groups.push_back({IV});
            }

            for (auto& group : groups)
            {
                if (group.size() <= 1)
                    continue;

                TACInductionVariable& primary = group[0];

                TACValue primaryStepDest = primary.stepInst->dest;

                for (size_t i = 1; i < group.size(); i++)
                {
                    TACInductionVariable& redundant = group[i];

                    for (auto& block : TACFunc->Blocks)
                    {
                        for (auto& inst : block->Instructions)
                        {
                            for (TACValue* use : GetTACInstOperands(inst.get()).Uses)
                            {
                                if (*use == redundant.phi->variable)
                                {
                                    *use = primary.phi->variable;
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return changed;
}
