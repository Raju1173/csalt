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

std::unordered_map<TACValue, TACValue> Copies;
std::unordered_map<ExpressionKey, TACValue> Expressions;
std::unordered_map<FunctionCallKey, TACValue> Calls; // All functions are guaranteed to be pure...

void WalkTACDomTree(TACBlock* block)
{
    std::vector<TACValue> addedCopies;
    std::vector<ExpressionKey> addedExpressions;
    std::vector<FunctionCallKey> addedCalls;

    for (auto& inst : block->Instructions)
    {
        if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
        {
            if (Copies.contains(assign->source))
            {
                assign->source = Copies[assign->source];
            }

            Copies[assign->dest] = assign->source;

            addedCopies.push_back(assign->dest);
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

                    Copies[assignInst->dest] = assignInst->source;
                    addedCopies.push_back(assignInst->dest);

                    inst = std::move(assignInst);
                }

                else if (Expressions.contains(key2))
                {
                    auto assignInst = std::make_unique<TACAssign>();

                    assignInst->dest = binary->dest;
                    assignInst->source = Expressions[key2];

                    Copies[assignInst->dest] = assignInst->source;
                    addedCopies.push_back(assignInst->dest);

                    inst = std::move(assignInst);
                }

                else
                {
                    Expressions[key1] = binary->dest;

                    addedExpressions.push_back(key1);
                }
            }

            else
            {
                if (Expressions.contains(key1))
                {
                    auto assignInst = std::make_unique<TACAssign>();

                    assignInst->dest = binary->dest;
                    assignInst->source = Expressions[key1];

                    Copies[assignInst->dest] = assignInst->source;
                    addedCopies.push_back(assignInst->dest);

                    inst = std::move(assignInst);
                }

                else
                {
                    Expressions[key1] = binary->dest;

                    addedExpressions.push_back(key1);
                }
            }
        }

        else if (auto call = dynamic_cast<TACCall*>(inst.get()))
        {
            if (call->dest.has_value())
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

                    Copies[assignInst->dest] = assignInst->source;
                    addedCopies.push_back(assignInst->dest);

                    inst = std::move(assignInst);
                }

                else
                {
                    Calls[key] = currentDest;
                    addedCalls.push_back(key);
                }
            }
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

    for (const auto& key : addedCopies)
        Copies.erase(key);
    for (const auto& key : addedExpressions)
        Expressions.erase(key);
    for (const auto& key : addedCalls)
        Calls.erase(key);
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
