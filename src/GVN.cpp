#include "TACGenerator.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

struct ExpressionKey
{
    BinaryOp op;
    TACValue left;
    TACValue right;

    auto operator<=>(const ExpressionKey&) const = default;
};

struct FunctionCallKey
{
    std::string function;
    std::vector<TACValue> args;

    auto operator<=>(const FunctionCallKey&) const = default;
};

std::map<TACValue, TACValue> Copies;
std::map<ExpressionKey, TACValue> Expressions;
std::map<FunctionCallKey, TACValue> Calls; // All functions are guaranteed to be pure...

void ComputeTACTransitions(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (auto& block : TACFunc->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (auto branch = dynamic_cast<TACBranch*>(inst.get()))
                {
                    TACBlock* trueTarget = (*(std::find_if(TACFunc->Blocks.begin(), TACFunc->Blocks.end(), [&branch](const auto& b) { return b->ID == branch->TrueTarget; }))).get();
                    TACBlock* falseTarget = (*(std::find_if(TACFunc->Blocks.begin(), TACFunc->Blocks.end(), [&branch](const auto& b) { return b->ID == branch->FalseTarget; }))).get();

                    block->Children.push_back(trueTarget);
                    block->Children.push_back(falseTarget);

                    trueTarget->Parents.push_back(block.get());
                    falseTarget->Parents.push_back(block.get());
                }

                else if (auto jump = dynamic_cast<TACJump*>(inst.get()))
                {
                    TACBlock* target = (*(std::find_if(TACFunc->Blocks.begin(), TACFunc->Blocks.end(), [&jump](const auto& b) { return b->ID == jump->TargetBlock; }))).get();

                    block->Children.push_back(target);

                    target->Parents.push_back(block.get());
                }
            }
        }
    }
}

void ComputeTACDominators(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    for (auto& FuncPtr : TAC)
    {
        if (!FuncPtr || FuncPtr->Blocks.empty())
            continue;

        auto& Blocks = FuncPtr->Blocks;

        TACBlock* entryBlock = Blocks[0].get();

        std::set<TACBlock*> universalSet;

        for (const auto& blockUniquePtr : Blocks)
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
                TACBlock* curBlock = Blocks[j].get();

                if (curBlock == entryBlock)
                    continue;

                std::set<TACBlock*> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = curBlock->Parents[0]->Dominators;

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::set<TACBlock*> currentIntersection;
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

void ComputeTACDominatorTree(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    TACBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    for (auto& TACFunc : TAC)
    {
        for (auto& block : TACFunc->Blocks)
        {
            for (TACBlock* dom : block->Dominators)
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

void WalkTACDomTree(TACBlock* block)
{
    for (auto& inst : block->Instructions)
    {
        if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
        {
            if (Copies.contains(assign->source))
            {
                assign->source = Copies[assign->source];
            }

            Copies[assign->dest] = assign->source;
        }

        else if (auto binary = dynamic_cast<TACBinaryOp*>(inst.get()))
        {
            if (Copies.contains(binary->left))
                binary->left = Copies[binary->left];

            if (Copies.contains(binary->right))
                binary->right = Copies[binary->right];

            ExpressionKey key1 = {binary->op, binary->left, binary->right};
            ExpressionKey key2 = {binary->op, binary->right, binary->left};

            if (binary->op == BinaryOp::PLUS || binary->op == BinaryOp::MUL)
            {
                if (Expressions.contains(key1))
                {
                    auto assignInst = std::make_unique<TACAssign>();

                    assignInst->dest = binary->dest;
                    assignInst->source = Expressions[key1];

                    inst = std::move(assignInst);
                }

                else if (Expressions.contains(key2))
                {
                    auto assignInst = std::make_unique<TACAssign>();

                    assignInst->dest = binary->dest;
                    assignInst->source = Expressions[key2];

                    inst = std::move(assignInst);
                }

                else
                {
                    Expressions[key1] = binary->dest;
                }
            }

            else
            {
                if (Expressions.contains(key1))
                {
                    Expressions[key1] = binary->dest;
                }
            }
        }

        else if (auto call = dynamic_cast<TACCall*>(inst.get()))
        {
            for (size_t i = 0; i < call->args.size(); i++)
            {
                if (Copies.contains(call->args[i]))
                {
                    call->args[i] = Copies[call->args[i]];
                }
            }

            FunctionCallKey key = {call->functionName, call->args};

            auto currentDest = call->dest.value();

            if (Calls.contains(key))
            {
                auto assignInst = std::make_unique<TACAssign>();

                assignInst->dest = call->dest.value();
                assignInst->source = Calls[key];

                inst = std::move(assignInst);
            }

            Calls[key] = currentDest;
        }

        else if (auto branch = dynamic_cast<TACBranch*>(inst.get()))
        {
            if (Copies.contains(branch->cond.Left))
                branch->cond.Left = Copies[branch->cond.Left];

            if (Copies.contains(branch->cond.Right))
                branch->cond.Right = Copies[branch->cond.Right];
        }

        else if (auto ret = dynamic_cast<TACReturn*>(inst.get()))
        {
            if (Copies.contains(ret->ReturnValue))
            {
                ret->ReturnValue = Copies[ret->ReturnValue];
            }
        }
    }

    for (auto domChild : block->DominatorTreeChildren)
    {
        WalkTACDomTree(domChild);
    }
}

void GVN(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    ComputeTACTransitions(TAC);
    ComputeTACDominators(TAC);
    ComputeTACDominatorTree(TAC);

    for (auto& TACFunc : TAC)
    {
        WalkTACDomTree(TACFunc->Blocks[0].get());
    }
}
