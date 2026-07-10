#include "TACGenerator.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

std::unordered_map<std::string, int> VarUses;
std::unordered_map<std::string, int> CallCount;

bool EliminateDeadInstructions(TACFunction& TACFunc, TACVarUsesInfo& VarUsesInfo)
{
    bool changed = false;

    for (auto& Block : TACFunc.Blocks)
    {
        size_t initialSize = Block->Instructions.size();

        std::erase_if(Block->Instructions, [&VarUsesInfo](const auto& inst) {
            std::string destVar;

            if (inst->type == TACType::ASSIGN)
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
                    return false;

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

bool MergeLinearBlocks(TACFunction& TACFunc)
{
    for (size_t i = 0; i < TACFunc.Blocks.size(); ++i)
    {
        auto& BlockA = TACFunc.Blocks[i];

        if (BlockA->Instructions.empty())
            continue;

        if (BlockA->Instructions.back()->type == TACType::JUMP)
        {
            auto* jump = static_cast<TACJump*>(BlockA->Instructions.back().get());
            size_t targetID = jump->TargetBlock;

            auto blockBIt = std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [targetID](const auto& b) { return b->ID == targetID; });

            if (blockBIt != TACFunc.Blocks.end())
            {
                auto& BlockB = *blockBIt;

                if (BlockB->Parents.size() == 1 && BlockB->Parents.front() == BlockA.get())
                {
                    BlockA->Instructions.pop_back();

                    for (auto& inst : BlockB->Instructions)
                    {
                        BlockA->Instructions.push_back(std::move(inst));
                    }

                    std::erase(BlockA->Children, BlockB.get());

                    for (TACBlock* child : BlockB->Children)
                    {
                        BlockA->Children.push_back(child);

                        std::replace(child->Parents.begin(), child->Parents.end(), BlockB.get(), BlockA.get());
                    }

                    BlockB->Parents.clear();
                    BlockB->Children.clear();

                    TACFunc.Blocks.erase(blockBIt);
                    return true;
                }
            }
        }
    }

    return false;
}

void EliminateDeadFunctions(TAC& TAC)
{
    std::unordered_map<std::string, size_t> callCounts;

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

    TAC.erase_if([&callCounts](const auto& TACFunc) {
        return TACFunc.Name != "main" && callCounts[TACFunc.Name] == 0;
    });
}

void RemoveDeadCode(TAC& TAC)
{
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
            }

            if (EliminateUnreachableBlocks(TACFunc))
            {
                TAC.getVarUsesInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorTreeInfo(TACFunc.Name).isValid = false;

                changed = true;
            }
        }

        bool blocksChanged = true;

        while (blocksChanged)
        {
            blocksChanged = false;

            if (MergeLinearBlocks(TACFunc))
            {
                TAC.getVarUsesInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorTreeInfo(TACFunc.Name).isValid = false;

                changed = true;
            }
        }
    }

    EliminateDeadFunctions(TAC);
}
