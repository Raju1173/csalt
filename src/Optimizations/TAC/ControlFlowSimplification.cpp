#include "TACGenerator.h"
#include "TACEditor.h"
#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <print>
#include <utility>
#include <vector>

bool MergeLinearBlocks(TACFunction* TACFunc)
{
    for (auto& curBlock : TACFunc->Blocks)
    {
        if (curBlock->Instructions.empty())
            continue;

        if (curBlock->Instructions.back()->type != TACType::JUMP)
            continue;

        TACJump* jump = static_cast<TACJump*>(curBlock->Instructions.back().get());

        if (jump->TargetBlock->Parents.size() != 1)
            continue;

        TACBlock* jumpBlock = std::find_if(TACFunc->Blocks.begin(), TACFunc->Blocks.end(), [&jump](const auto& b) { return b.get() == jump->TargetBlock; })->get();

        TACEditor::deleteInstruction(curBlock.get(), curBlock->Instructions.back().get(), Message{IRPass::CONTROL_FLOW_SIMPLIFICATION, IRTransformType::DELETED, std::format("jump redundant after target block 'Block - {}' got merged with this block", jumpBlock->ID)});

        for (int i = jumpBlock->Instructions.size() - 1; i >= 0; --i)
        {
            auto& inst = jumpBlock->Instructions[i];

            TACEditor::moveInstructionTo(jumpBlock, inst.get(), curBlock.get(), Message{IRPass::CONTROL_FLOW_SIMPLIFICATION, IRTransformType::MOVED, std::format("merged from linear block 'Block - {}' into 'Block - {}'", jumpBlock->ID, curBlock->ID)});
        }

        auto childrenCopy = jumpBlock->Children;

        TACEditor::removeEdge(curBlock.get(), jumpBlock);

        for (TACBlock* child : childrenCopy)
        {
            TACEditor::removeEdge(jumpBlock, child);
            TACEditor::addEdge(curBlock.get(), child);
        }

        TACEditor::deleteBlock(jumpBlock, Message{IRPass::CONTROL_FLOW_SIMPLIFICATION, IRTransformType::DELETED, std::format("merged linear block into 'Block - {}'", curBlock->ID)});

        return true;
    }

    return false;
}

bool EliminateEmptyJumpBlocks(TACFunction* TACFunc)
{
    for (auto& emptyBlock : TACFunc->Blocks)
    {
        if (emptyBlock == TACFunc->Blocks.front())
            continue;

        if (emptyBlock->Instructions.size() != 1 || emptyBlock->Instructions.back()->type != TACType::JUMP)
            continue;

        TACJump* jump = static_cast<TACJump*>(emptyBlock->Instructions.back().get());
        TACBlock* targetBlock = jump->TargetBlock;

        if (targetBlock == emptyBlock.get())
            continue;

        auto parentsCopy = emptyBlock->Parents;

        for (TACBlock* parent : parentsCopy)
        {
            TACEditor::removeEdge(parent, emptyBlock.get());
            TACEditor::addEdge(parent, targetBlock);

            if (parent->Instructions.back()->type == TACType::JUMP)
            {
                auto* parentJump = static_cast<TACJump*>(parent->Instructions.back().get());

                if (parentJump->TargetBlock == emptyBlock.get())
                    parentJump->TargetBlock = targetBlock;
            }

            else if (parent->Instructions.back()->type == TACType::BRANCH)
            {
                auto* parentBranch = static_cast<TACBranch*>(parent->Instructions.back().get());

                if (parentBranch->TrueTarget == emptyBlock.get())
                    parentBranch->TrueTarget = targetBlock;

                if (parentBranch->FalseTarget == emptyBlock.get())
                    parentBranch->FalseTarget = targetBlock;
            }
        }

        TACEditor::deleteBlock(emptyBlock.get(), Message{IRPass::CONTROL_FLOW_SIMPLIFICATION, IRTransformType::DELETED, std::format("eliminated empty jump block, redirected parents to Block - {}", targetBlock->ID)});

        return true;
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

            // bitwise OR operator "|" makes both functions run whereas comparison OR operator "||" skips checking second condition if first condition evaluates to true...
            if (MergeLinearBlocks(TACFunc.get()) | EliminateEmptyJumpBlocks(TACFunc.get()))
            {
                TACEditor::reconstructSSA(TAC);

                blocksChanged = true;
                globalChanged = true;
            }
        }
    }

    return globalChanged;
}
