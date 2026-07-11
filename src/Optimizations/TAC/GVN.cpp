#include "TACGenerator.h"
#include <map>
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

bool WalkTACDomTree(TACBlock* block, TACDominatorTreeInfo& DomTreeInfo)
{
    bool changed = false;

    std::vector<TACValue> addedCopies;
    std::vector<ExpressionKey> addedExpressions;
    std::vector<FunctionCallKey> addedCalls;

    for (auto& inst : block->Instructions)
    {
        switch (inst->type)
        {
            case TACType::ASSIGN:
                {
                    TACAssign* assign = static_cast<TACAssign*>(inst.get());

                    if (Copies.contains(assign->source))
                    {
                        assign->source = Copies[assign->source];

                        changed = true;
                    }

                    Copies[assign->dest] = assign->source;

                    addedCopies.push_back(assign->dest);
                }
                break;

            case TACType::BINARYOP:
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    if (Copies.contains(binary->left))
                    {
                        binary->left = Copies[binary->left];

                        changed = true;
                    }

                    if (Copies.contains(binary->right))
                    {
                        binary->right = Copies[binary->right];

                        changed = true;
                    }

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

                            changed = true;
                        }

                        else if (Expressions.contains(key2))
                        {
                            auto assignInst = std::make_unique<TACAssign>();

                            assignInst->dest = binary->dest;
                            assignInst->source = Expressions[key2];

                            Copies[assignInst->dest] = assignInst->source;
                            addedCopies.push_back(assignInst->dest);

                            inst = std::move(assignInst);

                            changed = true;
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

                            changed = true;
                        }

                        else
                        {
                            Expressions[key1] = binary->dest;

                            addedExpressions.push_back(key1);
                        }
                    }
                }
                break;

            case TACType::CALL:
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    if (call->dest.has_value())
                    {
                        for (size_t i = 0; i < call->args.size(); i++)
                        {
                            if (Copies.contains(call->args[i]))
                            {
                                call->args[i] = Copies[call->args[i]];

                                changed = true;
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

                            changed = true;
                        }

                        else
                        {
                            Calls[key] = currentDest;
                            addedCalls.push_back(key);
                        }
                    }
                }
                break;

            case TACType::BRANCH:
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    if (Copies.contains(branch->cond.Left))
                    {
                        branch->cond.Left = Copies[branch->cond.Left];

                        changed = true;
                    }

                    if (Copies.contains(branch->cond.Right))
                    {
                        branch->cond.Right = Copies[branch->cond.Right];

                        changed = true;
                    }
                }
                break;

            case TACType::RETURN:
                {
                    TACReturn* ret = static_cast<TACReturn*>(inst.get());

                    if (Copies.contains(ret->ReturnValue))
                    {
                        ret->ReturnValue = Copies[ret->ReturnValue];

                        changed = true;
                    }
                }
                break;
        }
    }

    for (auto domChild : DomTreeInfo.DominatorTree[block])
    {
        changed = WalkTACDomTree(domChild, DomTreeInfo);
    }

    for (const auto& key : addedCopies)
        Copies.erase(key);
    for (const auto& key : addedExpressions)
        Expressions.erase(key);
    for (const auto& key : addedCalls)
        Calls.erase(key);

    return changed;
}

bool GVN(TAC& TAC)
{
    bool changed = false;

    TAC.computeDominators();

    for (TACFunction& TACFunc : TAC)
    {
        Copies.clear();
        Expressions.clear();
        Calls.clear();

        if (!TACFunc.Blocks.empty())
        {
            changed = WalkTACDomTree(TACFunc.Blocks[0].get(), TAC.computeDominatorTree(TACFunc));
        }

        if (changed)
            TAC.getVarUsesInfo(TACFunc.Name).isValid = false;
    }

    return changed;
}
