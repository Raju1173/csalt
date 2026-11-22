#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <print>
#include <vector>

bool MergeLinearBlocks(TACFunction* TACFunc)
{
    for (auto& curBlock : TACFunc->Blocks)
    {
        if (curBlock->Instructions.empty())
            continue;

        if (curBlock->Instructions.back()->type != TACInstType::JUMP)
            continue;

        TACJump* jump = static_cast<TACJump*>(curBlock->Instructions.back().get());

        if (jump->TargetBlock->Parents.size() != 1)
            continue;

        TACBlock* jumpBlock = jump->TargetBlock;

        IREditor<TACTypes>::deleteInstruction(curBlock.get(), curBlock->Instructions.back().get(), Message{Phase::CFG_SIMP, IRTransformType::DELETED, std::format("jump redundant after target block 'Block - {}' got merged with this block", jumpBlock->ID)});

        IREditor<TACTypes>::removeEdge(curBlock.get(), jumpBlock);
        IREditor<TACTypes>::removePhiSource(jumpBlock, curBlock.get());

        while (!jumpBlock->Instructions.empty())
        {
            IREditor<TACTypes>::moveInstructionTo(jumpBlock, jumpBlock->Instructions[0].get(), curBlock.get(), Message{Phase::CFG_SIMP, IRTransformType::MOVED, std::format("merged from linear block 'Block - {}' into 'Block - {}'", jumpBlock->ID, curBlock->ID)});
        }

        auto childrenCopy = jumpBlock->Children;

        for (TACBlock* child : childrenCopy)
        {
            IREditor<TACTypes>::removeEdge(jumpBlock, child);
            IREditor<TACTypes>::addEdge(curBlock.get(), child);
            IREditor<TACTypes>::remapPhiSources(child, jumpBlock, {curBlock.get()});
        }

        IREditor<TACTypes>::deleteBlock(jumpBlock, Message{Phase::CFG_SIMP, IRTransformType::DELETED, std::format("merged linear block into 'Block - {}'", curBlock->ID)});

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

        if (emptyBlock->Instructions.size() != 1 || emptyBlock->Instructions.back()->type != TACInstType::JUMP)
            continue;

        TACJump* jump = static_cast<TACJump*>(emptyBlock->Instructions.back().get());

        if (jump->TargetBlock == emptyBlock.get())
            continue;

        auto parentsCopy = emptyBlock->Parents;

        IREditor<TACTypes>::remapPhiSources(jump->TargetBlock, emptyBlock.get(), parentsCopy);

        for (TACBlock* parent : parentsCopy)
        {
            if (parent->Instructions.back()->type == TACInstType::JUMP)
            {
                TACJump* parentJump = static_cast<TACJump*>(parent->Instructions.back().get());

                if (parentJump->TargetBlock == emptyBlock.get())
                {
                    IREditor<TACTypes>::addMessage(parentJump, Message{Phase::CFG_SIMP, IRTransformType::MODIFIED, std::format("redirected jump to an empty block B{}, directly to that block's target B{}", parentJump->TargetBlock->ID, jump->TargetBlock->ID)});

                    parentJump->TargetBlock = jump->TargetBlock;
                }
            }

            else if (parent->Instructions.back()->type == TACInstType::BRANCH)
            {
                TACBranch* parentBranch = static_cast<TACBranch*>(parent->Instructions.back().get());

                if (parentBranch->TrueTarget == emptyBlock.get())
                {
                    IREditor<TACTypes>::addMessage(parentBranch, Message{Phase::CFG_SIMP, IRTransformType::MODIFIED, std::format("redirected branch to an empty block B{}, directly to that block's target B{}", parentBranch->TrueTarget->ID, jump->TargetBlock->ID)});

                    parentBranch->TrueTarget = jump->TargetBlock;
                }

                if (parentBranch->FalseTarget == emptyBlock.get())
                {
                    IREditor<TACTypes>::addMessage(parentBranch, Message{Phase::CFG_SIMP, IRTransformType::MODIFIED, std::format("redirected branch to an empty block B{}, directly to that block's target B{}", parentBranch->TrueTarget->ID, jump->TargetBlock->ID)});

                    parentBranch->FalseTarget = jump->TargetBlock;
                }
            }

            IREditor<TACTypes>::removeEdge(parent, emptyBlock.get());
            IREditor<TACTypes>::addEdge(parent, jump->TargetBlock);
        }

        IREditor<TACTypes>::removeEdge(emptyBlock.get(), jump->TargetBlock);
        IREditor<TACTypes>::removePhiSource(jump->TargetBlock, emptyBlock.get());

        IREditor<TACTypes>::deleteBlock(emptyBlock.get(), Message{Phase::CFG_SIMP, IRTransformType::DELETED, std::format("eliminated empty jump block, redirected parents to Block - {}", jump->TargetBlock->ID)});

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

            // using bitwise OR operator to make both functions run...
            if (MergeLinearBlocks(TACFunc.get()) | EliminateEmptyJumpBlocks(TACFunc.get()))
            {
                blocksChanged = true;
                globalChanged = true;
            }
        }
    }

    return globalChanged;
}
