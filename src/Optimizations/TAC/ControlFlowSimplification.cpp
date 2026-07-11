#include "CFGBuilder.h"
#include "TACGenerator.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

bool MergeLinearBlocks(TACFunction& TACFunc)
{
    for (size_t i = 0; i < TACFunc.Blocks.size(); ++i)
    {
        auto& curBlock = TACFunc.Blocks[i];

        if (curBlock->Instructions.empty())
            continue;

        if (curBlock->Instructions.back()->type == TACType::JUMP)
        {
            TACJump* jump = static_cast<TACJump*>(curBlock->Instructions.back().get());

            auto blockBIt = std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [&jump](const auto& b) { return b->ID == jump->TargetBlock; });

            if (blockBIt != TACFunc.Blocks.end())
            {
                auto& jumpBlock = *blockBIt;

                if (jumpBlock->Parents.size() == 1 && jumpBlock->Parents.front() == curBlock.get())
                {
                    curBlock->Instructions.pop_back();

                    for (auto& inst : jumpBlock->Instructions)
                    {
                        curBlock->Instructions.push_back(std::move(inst));
                    }

                    std::erase(curBlock->Children, jumpBlock.get());

                    for (TACBlock* child : jumpBlock->Children)
                    {
                        if (child == jumpBlock.get())
                            continue;

                        curBlock->Children.push_back(child);

                        auto pIt = std::find(child->Parents.begin(), child->Parents.end(), curBlock.get());
                        auto bIt = std::find(child->Parents.begin(), child->Parents.end(), jumpBlock.get());

                        if (bIt != child->Parents.end())
                        {
                            if (pIt != child->Parents.end())
                            {
                                child->Parents.erase(bIt);
                            }

                            else
                            {
                                *bIt = curBlock.get();
                            }
                        }
                    }

                    TACFunc.Blocks.erase(blockBIt);

                    return true;
                }
            }
        }
    }

    return false;
}

bool EliminateEmptyJumpBlocks(TACFunction& TACFunc)
{
    for (auto& jumpBlock : TACFunc.Blocks)
    {
        if (jumpBlock == TACFunc.Blocks.front())
            continue;

        if (jumpBlock->Instructions.size() == 1 && jumpBlock->Instructions.back()->type == TACType::JUMP)
        {
            TACJump* jump = static_cast<TACJump*>(jumpBlock->Instructions.back().get());

            auto blockBIt = std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [&jump](const auto& b) { return b->ID == jump->TargetBlock; });

            if (blockBIt != TACFunc.Blocks.end())
            {
                auto& parentBlock = *blockBIt;

                if (parentBlock.get() == jumpBlock.get())
                    continue;

                for (TACBlock* parent : jumpBlock->Parents)
                {
                    if (!parent->Instructions.empty())
                    {
                        auto& lastInst = parent->Instructions.back();

                        if (lastInst->type == TACType::JUMP)
                        {
                            TACJump* pJump = static_cast<TACJump*>(lastInst.get());

                            if (pJump->TargetBlock == jumpBlock->ID)
                                pJump->TargetBlock = parentBlock->ID;
                        }
                        else if (lastInst->type == TACType::BRANCH)
                        {
                            TACBranch* pBranch = static_cast<TACBranch*>(lastInst.get());

                            if (pBranch->TrueTarget == jumpBlock->ID)
                                pBranch->TrueTarget = parentBlock->ID;

                            if (pBranch->FalseTarget == jumpBlock->ID)
                                pBranch->FalseTarget = parentBlock->ID;
                        }
                    }

                    std::replace(parent->Children.begin(), parent->Children.end(), jumpBlock.get(), parentBlock.get());

                    if (std::find(parentBlock->Parents.begin(), parentBlock->Parents.end(), parent) == parentBlock->Parents.end())
                    {
                        parentBlock->Parents.push_back(parent);
                    }

                    for (auto& inst : parentBlock->Instructions)
                    {
                        if (inst->type != TACType::PHI)
                            break;

                        auto* phi = static_cast<TACPhi*>(inst.get());

                        auto argIt = std::find_if(phi->args.begin(), phi->args.end(), [&jumpBlock](const PhiArgument& arg) { return arg.SourceID == jumpBlock->ID; });

                        if (argIt != phi->args.end())
                        {
                            auto parentArgIt = std::find_if(phi->args.begin(), phi->args.end(), [parent](const PhiArgument& arg) { return arg.SourceID == parent->ID; });

                            if (parentArgIt != phi->args.end())
                                parentArgIt->Value = argIt->Value;

                            else
                                phi->args.push_back({parent->ID, argIt->Value});
                        }
                    }
                }

                for (auto& inst : parentBlock->Instructions)
                {
                    if (inst->type != TACType::PHI)
                        break;

                    auto* phi = static_cast<TACPhi*>(inst.get());

                    auto eraseIt = std::find_if(phi->args.begin(), phi->args.end(), [&jumpBlock](const PhiArgument& arg) { return arg.SourceID == jumpBlock->ID; });

                    if (eraseIt != phi->args.end())
                    {
                        phi->args.erase(eraseIt);
                    }
                }

                std::erase(parentBlock->Parents, jumpBlock.get());

                std::erase_if(TACFunc.Blocks, [&jumpBlock](const auto& b) { return b.get() == jumpBlock.get(); });

                return true;
            }
        }
    }

    return false;
}

bool SimplifyControlFlow(TAC& TAC)
{
    bool globalChanged = false;

    for (auto& TACFunc : TAC)
    {
        bool blocksChanged = true;

        while (blocksChanged)
        {
            blocksChanged = false;

            if (MergeLinearBlocks(TACFunc))
            {
                TAC.getDominatorInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorTreeInfo(TACFunc.Name).isValid = false;

                blocksChanged = true;
                globalChanged = true;
            }

            if (EliminateEmptyJumpBlocks(TACFunc))
            {
                TAC.getVarUsesInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorInfo(TACFunc.Name).isValid = false;
                TAC.getDominatorTreeInfo(TACFunc.Name).isValid = false;

                blocksChanged = true;
                globalChanged = true;
            }
        }
    }

    return globalChanged;
}
