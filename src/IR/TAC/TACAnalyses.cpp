#include "TACGenerator.h"
#include "TACInstructions.h"

TACDominatorInfo& TACFunction::getDominatorInfo()
{
    TACDominatorInfo& DomInfo = Metadata.DomInfo;

    if (!DomInfo.isValid)
    {
        DomInfo.isValid = true;

        DomInfo.Dominators.clear();

        if (Blocks.empty())
            return DomInfo;

        TACBlock* entryBlock = Blocks[0].get();

        std::unordered_set<TACBlock*> universalSet;

        for (auto& block : Blocks)
        {
            universalSet.insert(block.get());
        }

        DomInfo.Dominators[entryBlock] = {entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            DomInfo.Dominators[Blocks[j].get()] = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                TACBlock* curBlock = Blocks[j].get();

                if (curBlock == entryBlock)
                    continue;

                std::unordered_set<TACBlock*> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = DomInfo.Dominators[curBlock->Parents[0]];

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::unordered_set<TACBlock*> currentIntersection;

                        for (TACBlock* dom : NewDominators)
                        {
                            if (DomInfo.Dominators[curBlock->Parents[k]].contains(dom))
                            {
                                currentIntersection.insert(dom);
                            }
                        }

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(curBlock);

                if (NewDominators != DomInfo.Dominators[curBlock])
                {
                    DomInfo.Dominators[curBlock] = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }

    return DomInfo;
}

TACDominatorTreeInfo& TACFunction::getDominatorTreeInfo()
{
    TACDominatorInfo& DomInfo = getDominatorInfo();
    TACDominatorTreeInfo& DomTreeInfo = Metadata.DomTreeInfo;

    TACBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    if (!DomTreeInfo.isValid)
    {
        DomTreeInfo.isValid = true;

        DomTreeInfo.DominatorTree.clear();

        for (auto& block : Blocks)
        {
            for (TACBlock* dom : DomInfo.Dominators[block.get()])
            {
                if (dom == block.get())
                    continue;

                size_t size = DomInfo.Dominators[dom].size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                DomTreeInfo.DominatorTree[nearestDominator].push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }

    return DomTreeInfo;
}

void ComputeBlockFrontiers(TACBlock* Block, TACDominatorInfo& DomInfo, TACDominatorTreeInfo& DomTreeInfo, TACFrontierInfo& FrontierInfo)
{
    if (!Block->Instructions.empty())
    {
        if (Block->Instructions.back()->type == TACInstType::JUMP)
        {
            TACJump* jump = static_cast<TACJump*>(Block->Instructions.back().get());

            if (!DomInfo.Dominators[jump->TargetBlock].contains(Block) || jump->TargetBlock == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(jump->TargetBlock);
            }
        }

        else if (Block->Instructions.back()->type == TACInstType::BRANCH)
        {
            TACBranch* br = static_cast<TACBranch*>(Block->Instructions.back().get());

            if (!DomInfo.Dominators[br->TrueTarget].contains(Block) || br->TrueTarget == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(br->TrueTarget);
            }

            if (!DomInfo.Dominators[br->FalseTarget].contains(Block) || br->FalseTarget == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(br->FalseTarget);
            }
        }
    }

    for (TACBlock* domChild : DomTreeInfo.DominatorTree[Block])
    {
        ComputeBlockFrontiers(domChild, DomInfo, DomTreeInfo, FrontierInfo);

        for (TACBlock* childFrontier : FrontierInfo.Frontiers[domChild])
        {
            if ((!DomInfo.Dominators[childFrontier].contains(Block) || childFrontier == Block))
            {
                FrontierInfo.Frontiers[Block].push_back(childFrontier);
            }
        }
    }
}

TACFrontierInfo& TACFunction::getFrontierInfo()
{
    TACDominatorInfo& domInfo = getDominatorInfo();
    TACDominatorTreeInfo& domTreeInfo = getDominatorTreeInfo();
    TACFrontierInfo& frontierInfo = Metadata.FrontierInfo;

    if (!frontierInfo.isValid)
    {
        frontierInfo.isValid = true;

        frontierInfo.Frontiers.clear();

        ComputeBlockFrontiers(Blocks[0].get(), domInfo, domTreeInfo, frontierInfo);
    }

    return frontierInfo;
}

TACVarUsesInfo& TACFunction::getVarUsesInfo()
{
    TACVarUsesInfo& VarUsesInfo = Metadata.VarUsesInfo;

    if (!VarUsesInfo.isValid)
    {
        VarUsesInfo.isValid = true;

        VarUsesInfo.VarUses.clear();

        for (const auto& Block : Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                for (TACValue* val : GetTACInstOperands(inst.get()).Uses)
                {
                    VarUsesInfo.VarUses[*val]++;
                }
            }
        }
    }

    return VarUsesInfo;
}

TACDefBlocksInfo& TACFunction::getDefBlocksInfo()
{
    TACDefBlocksInfo& DefBlocksInfo = Metadata.DefBlocksInfo;

    if (!DefBlocksInfo.isValid)
    {
        DefBlocksInfo.isValid = true;

        DefBlocksInfo.DefBlocks.clear();

        for (const auto& Block : Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                if (GetTACInstOperands(inst.get()).Def != nullptr)
                {
                    DefBlocksInfo.DefBlocks[*GetTACInstOperands(inst.get()).Def].insert(Block.get());
                }
            }
        }
    }

    return DefBlocksInfo;
}

void findLoopBlocks(TACLoop& loop, TACBlock* block)
{
    if (block == loop.Header)
        return;

    for (TACBlock* parent : block->Parents)
    {
        if (loop.Blocks.contains(parent))
            continue;

        loop.Blocks.insert(parent);

        findLoopBlocks(loop, parent);
    }
}

TACLoopInfo& TACFunction::getLoopInfo()
{
    TACLoopInfo& LoopInfo = Metadata.LoopInfo;
    TACDominatorInfo& DominatorInfo = getDominatorInfo();

    if (!LoopInfo.isValid)
    {
        LoopInfo.isValid = true;

        LoopInfo.Loops.clear();

        for (auto& Block : Blocks)
        {
            for (TACBlock* child : Block->Children)
            {
                if (DominatorInfo.Dominators[Block.get()].contains(child))
                {
                    // csalt doesnt support continue, break or goto, thus, every single loop is guaranteed to have only one backedge...
                    LoopInfo.Loops.push_back(TACLoop{child, Block.get(), {child, Block.get()}});

                    findLoopBlocks(LoopInfo.Loops.back(), Block.get());
                }
            }
        }
    }

    return LoopInfo;
}
