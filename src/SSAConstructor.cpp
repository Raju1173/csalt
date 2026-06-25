#include "SSAConstructor.h"
#include "CFGBuilder.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <utility>

void ComputeDominators(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (auto &FuncPtr : CFG)
    {
        if (!FuncPtr || FuncPtr->Blocks.empty())
            continue;

        auto &Blocks = FuncPtr->Blocks;

        CFGBlock *entryBlock = Blocks[0].get();

        std::set<CFGBlock *> universalSet;

        for (const auto &blockUniquePtr : Blocks)
        {
            universalSet.insert(blockUniquePtr.get());
        }

        entryBlock->Dominators = {entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            Blocks[j]->Dominators = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                CFGBlock *curBlock = Blocks[j].get();

                if (curBlock == entryBlock)
                    continue;

                std::set<CFGBlock *> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = curBlock->Parents[0]->Dominators;

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::set<CFGBlock *> currentIntersection;
                        std::set_intersection(NewDominators.begin(), NewDominators.end(), curBlock->Parents[k]->Dominators.begin(), curBlock->Parents[k]->Dominators.end(), std::inserter(currentIntersection, currentIntersection.begin()));

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(curBlock);

                if (NewDominators != curBlock->Dominators)
                {
                    curBlock->Dominators = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }
}

void ComputeDominatorTree(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    CFGBlock *nearestDominator = nullptr;
    size_t maxSize = 0;

    for (auto &CFGFunc : CFG)
    {
        for (auto &block : CFGFunc->Blocks)
        {
            for (CFGBlock *dom : block->Dominators)
            {
                if (dom == block.get())
                    continue;

                size_t size = dom->Dominators.size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                nearestDominator->DominatorTreeChildren.push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }
}

void ComputeFrontiers(CFGBlock *Block)
{
    if (Block->TransitionNext != nullptr)
    {
        if (!Block->TransitionNext->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionNext);
        }
    }

    if (Block->TransitionTrue != nullptr)
    {
        if (!Block->TransitionTrue->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionTrue);
        }
    }

    if (Block->TransitionFalse != nullptr)
    {
        if (!Block->TransitionFalse->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionFalse);
        }
    }

    for (auto domChild : Block->DominatorTreeChildren)
    {
        ComputeFrontiers(domChild);

        for (auto childFrontier : domChild->Frontiers)
        {
            if (!childFrontier->Dominators.contains(Block) || childFrontier == Block)
            {
                Block->Frontiers.insert(childFrontier);
            }
        }
    }
}
