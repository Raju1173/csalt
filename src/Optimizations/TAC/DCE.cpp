#include "TACGenerator.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

bool EliminateDeadInstructions(TACFunction& TACFunc, TACVarUsesInfo& VarUsesInfo)
{
    bool changed = false;

    for (auto& Block : TACFunc.Blocks)
    {
        size_t initialSize = Block->Instructions.size();

        std::erase_if(Block->Instructions, [&VarUsesInfo](const auto& inst) {
            std::string destVar;

            if (inst->type == TACType::PHI)
            {
                TACPhi* phi = static_cast<TACPhi*>(inst.get());

                destVar = phi->variable.value;
            }

            else if (inst->type == TACType::ASSIGN)
            {
                TACAssign* assign = static_cast<TACAssign*>(inst.get());

                destVar = assign->dest.value;
            }

            else if (inst->type == TACType::BINARYOP)
            {
                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                destVar = binary->dest.value;
            }

            else if (inst->type == TACType::CALL)
            {
                TACCall* call = static_cast<TACCall*>(inst.get());

                if (!call->dest.has_value())
                    return true;

                destVar = call->dest->value;
            }

            else
            {
                return false;
            }

            return !VarUsesInfo.VarUses.contains(destVar) || VarUsesInfo.VarUses[destVar] == 0;
        });

        if (Block->Instructions.size() != initialSize)
        {
            changed = true;
        }
    }

    return changed;
}

bool EliminateUnreachableBlocks(TACFunction& TACFunc)
{
    if (TACFunc.Blocks.empty())
        return false;

    size_t entryBlockID = TACFunc.Blocks.front()->ID;
    size_t initialBlocks = TACFunc.Blocks.size();

    std::erase_if(TACFunc.Blocks, [entryBlockID](const auto& Block) {
        if (Block->ID == entryBlockID)
            return false;

        return Block->Parents.empty();
    });

    return TACFunc.Blocks.size() != initialBlocks;
}

bool EliminateDeadFunctions(TAC& TAC)
{
    std::unordered_map<std::string, size_t> callCounts;

    size_t initialFuncCount = TAC.size();

    for (const auto& TACFunc : TAC)
    {
        for (const auto& Block : TACFunc.Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                if (inst->type == TACType::CALL)
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    callCounts[call->functionName]++;
                }
            }
        }
    }

    TAC.eraseFuncIf([&callCounts](const auto& TACFunc) {
        return TACFunc.Name != "main" && callCounts[TACFunc.Name] == 0;
    });

    return initialFuncCount != TAC.size();
}

bool RemoveDeadCode(TAC& TAC)
{
    bool globalChanged = false;

    for (auto& TACFunc : TAC)
    {
        bool changed = true;

        while (changed)
        {
            changed = false;

            if (EliminateDeadInstructions(TACFunc, TAC.computeVarUses(TACFunc)))
            {
                TAC.getVarUsesInfo(TACFunc.Name).isValid = false;

                changed = true;
                globalChanged = true;
            }

            if (EliminateUnreachableBlocks(TACFunc))
            {
                TAC.getVarUsesInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorTreeInfo(TACFunc.Name).isValid = false;

                changed = true;
                globalChanged = true;
            }
        }
    }

    return globalChanged || EliminateDeadFunctions(TAC);
}
