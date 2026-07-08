#include "TACGenerator.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct BlockTransitions
{
    std::unordered_set<int> parentIDs;
    std::unordered_set<int> childrenIDs;
};

std::unordered_map<std::string, int> VarUses;
std::unordered_map<int, BlockTransitions> BlockPreds;

std::unordered_map<std::string, int> CallCount;

void RemoveDeadCodeAndMergeBlocks(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    for (auto& TACFunc : TAC)
    {
        CallCount[TACFunc->Name] = 0;

        bool changed = true;

        while (changed)
        {
            changed = false;

            VarUses.clear();
            BlockPreds.clear();

            for (auto& Block : TACFunc->Blocks)
            {
                for (auto& inst : Block->Instructions)
                {
                    if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
                    {
                        VarUses[assign->dest.value] = 0;
                        VarUses[assign->source.value]++;
                    }

                    else if (auto binary = dynamic_cast<TACBinaryOp*>(inst.get()))
                    {
                        VarUses[binary->dest.value] = 0;
                        VarUses[binary->left.value]++;
                        VarUses[binary->right.value]++;
                    }

                    else if (auto call = dynamic_cast<TACCall*>(inst.get()))
                    {
                        if (call->dest.has_value())
                            VarUses[call->dest->value] = 0;
                    }

                    else if (auto branch = dynamic_cast<TACBranch*>(inst.get()))
                    {
                        VarUses[branch->cond.Left.value]++;
                        VarUses[branch->cond.Right.value]++;

                        BlockPreds[branch->TrueTarget].parentIDs.insert(Block->ID);
                        BlockPreds[branch->FalseTarget].parentIDs.insert(Block->ID);

                        BlockPreds[Block->ID].childrenIDs.insert(branch->TrueTarget);
                        BlockPreds[Block->ID].childrenIDs.insert(branch->FalseTarget);
                    }

                    else if (auto jump = dynamic_cast<TACJump*>(inst.get()))
                    {
                        BlockPreds[jump->TargetBlock].parentIDs.insert(Block->ID);

                        BlockPreds[Block->ID].childrenIDs.insert(jump->TargetBlock);
                    }

                    else if (auto ret = dynamic_cast<TACReturn*>(inst.get()))
                    {
                        VarUses[ret->ReturnValue.value]++;
                    }
                }
            }

            for (auto& Block : TACFunc->Blocks)
            {
                size_t initialSize = Block->Instructions.size();

                std::erase_if(Block->Instructions, [](const auto& inst) {
                    if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
                    {
                        auto var = VarUses.find(assign->dest.value);

                        return (var == VarUses.end() || var->second == 0);
                    }

                    if (auto binary = dynamic_cast<TACBinaryOp*>(inst.get()))
                    {
                        auto var = VarUses.find(binary->dest.value);

                        return (var == VarUses.end() || var->second == 0);
                    }

                    if (auto call = dynamic_cast<TACCall*>(inst.get()))
                    {
                        if (!call->dest.has_value())
                        {
                            return true;
                        }

                        auto var = VarUses.find(call->dest->value);

                        return (var == VarUses.end() || var->second == 0);
                    }

                    return false;
                });

                if (Block->Instructions.size() != initialSize)
                {
                    changed = true;
                }
            }

            if (TACFunc->Blocks.empty())
                continue;

            size_t entryBlockID = TACFunc->Blocks.front()->ID;

            std::erase_if(TACFunc->Blocks, [entryBlockID](const auto& Block) {
                if (Block->ID == entryBlockID)
                    return false;

                auto it = BlockPreds.find(Block->ID);

                if (it == BlockPreds.end() || it->second.parentIDs.empty())
                {
                    if (it != BlockPreds.end())
                    {
                        for (int childID : it->second.childrenIDs)
                        {
                            BlockPreds[childID].parentIDs.erase(Block->ID);
                        }
                    }

                    return true;
                }

                return false;
            });
        }

        bool blocksChanged = true;

        while (blocksChanged)
        {
            blocksChanged = false;

            for (size_t i = 0; i < TACFunc->Blocks.size(); ++i)
            {
                auto& BlockA = TACFunc->Blocks[i];

                if (BlockA->Instructions.empty())
                    continue;

                if (auto jump = dynamic_cast<TACJump*>(BlockA->Instructions.back().get()))
                {
                    size_t targetID = jump->TargetBlock;

                    if (BlockPreds[targetID].parentIDs.size() == 1 && BlockPreds[targetID].parentIDs.contains(BlockA->ID))
                    {
                        auto blockB = std::find_if(TACFunc->Blocks.begin(), TACFunc->Blocks.end(), [targetID](const auto& b) { return b->ID == targetID; });

                        if (blockB != TACFunc->Blocks.end())
                        {
                            auto& BlockB = *blockB;

                            BlockA->Instructions.pop_back();

                            for (auto& inst : BlockB->Instructions)
                            {
                                BlockA->Instructions.push_back(std::move(inst));
                            }

                            BlockPreds[BlockA->ID].childrenIDs = BlockPreds[targetID].childrenIDs;

                            for (int childID : BlockPreds[targetID].childrenIDs)
                            {
                                BlockPreds[childID].parentIDs.erase(targetID);
                                BlockPreds[childID].parentIDs.insert(BlockA->ID);
                            }

                            TACFunc->Blocks.erase(blockB);

                            blocksChanged = true;

                            break;
                        }
                    }
                }
            }
        }

        for (auto& Block : TACFunc->Blocks)
        {
            for (auto& inst : Block->Instructions)
            {
                if (auto call = dynamic_cast<TACCall*>(inst.get()))
                {
                    CallCount[call->functionName]++;
                }
            }
        }
    }

    std::erase_if(TAC, [](const auto& TACFunc) { return TACFunc->Name != "main" && CallCount[TACFunc->Name] == 0; });
}
