#include "CFGBuilder.h"
#include "MIRInstructions.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <vector>
#include <map>

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
std::map<TACValue, TACValue> NegDefs;
std::map<FunctionCallKey, TACValue> Calls; // all functions are guaranteed to be pure in the supported C subset...

bool WalkTACDomTree(TACBlock* block, TACDominatorTreeInfo& DomTreeInfo)
{
    bool changed = false;

    std::vector<TACValue> addedCopies;
    std::vector<ExpressionKey> addedExpressions;
    std::vector<TACValue> addedNegDefs;
    std::vector<FunctionCallKey> addedCalls;

    auto tryPropagate = [](TACValue& val) {
        auto it = Copies.find(val);

        if (it != Copies.end())
        {
            val = it->second;
            return true;
        }

        return false;
    };

    auto replaceWithAssign = [&block, &addedCopies, &changed](std::unique_ptr<TACInstruction>& inst, const TACValue& dest, const TACValue& source) {
        IREditor<TACTypes>::replaceInstruction(block, inst.get(), std::make_unique<TACAssign>(dest, source), Message{Phase::GVN, IRTransformType::REPLACED, std::format("TODO: ADD MESSAGE")});

        Copies[dest] = source;
        addedCopies.push_back(dest);

        changed = true;
    };

    for (auto& inst : block->Instructions)
    {
        switch (inst->type)
        {
            case TACInstType::ASSIGN:
                {
                    TACAssign* assign = static_cast<TACAssign*>(inst.get());

                    changed |= tryPropagate(assign->source);

                    Copies[assign->dest] = assign->source;
                    addedCopies.push_back(assign->dest);
                }
                break;

            case TACInstType::NEG:
                {
                    TACNeg* neg = static_cast<TACNeg*>(inst.get());

                    changed |= tryPropagate(neg->source);

                    if (NegDefs.contains(neg->source))
                        replaceWithAssign(inst, neg->dest, NegDefs[neg->source]);

                    else
                    {
                        NegDefs[neg->source] = neg->dest;
                        addedNegDefs.push_back(neg->source);
                    }
                }
                break;

            case TACInstType::BINARYOP:
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    changed |= tryPropagate(binary->left);
                    changed |= tryPropagate(binary->right);

                    TACValue left = binary->left;
                    TACValue right = binary->right;

                    if ((binary->op == BinaryOp::PLUS || binary->op == BinaryOp::MUL || binary->op == BinaryOp::DOUBLE_EQUAL || binary->op == BinaryOp::NOT_EQUAL) && left > right)
                    {
                        std::swap(left, right);
                    }

                    ExpressionKey key = {binary->op, left, right};

                    if (Expressions.contains(key))
                        replaceWithAssign(inst, binary->dest, Expressions[key]);

                    else
                    {
                        Expressions[key] = binary->dest;
                        addedExpressions.push_back(key);
                    }
                }
                break;

            case TACInstType::CALL:
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    if (call->dest.has_value())
                    {
                        for (auto& arg : call->args)
                            changed |= tryPropagate(arg);

                        FunctionCallKey key = {call->functionName, call->args};
                        auto currentDest = call->dest.value();

                        if (Calls.contains(key))
                            replaceWithAssign(inst, currentDest, Calls[key]);

                        else
                        {
                            Calls[key] = currentDest;
                            addedCalls.push_back(key);
                        }
                    }
                }
                break;

            case TACInstType::BRANCH:
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    changed |= tryPropagate(branch->cond.Left);
                    changed |= tryPropagate(branch->cond.Right);
                }
                break;

            case TACInstType::RETURN:
                {
                    TACReturn* ret = static_cast<TACReturn*>(inst.get());

                    if (ret->ReturnValue.has_value())
                        changed |= tryPropagate(ret->ReturnValue.value());
                }
                break;
        }
    }

    for (TACBlock* domChild : DomTreeInfo.DominatorTree[block])
    {
        changed |= WalkTACDomTree(domChild, DomTreeInfo);
    }

    for (TACValue& key : addedCopies)
        Copies.erase(key);
    for (ExpressionKey& key : addedExpressions)
        Expressions.erase(key);
    for (TACValue& key : addedNegDefs)
        NegDefs.erase(key);
    for (FunctionCallKey& key : addedCalls)
        Calls.erase(key);

    return changed;
}

bool GVN(TAC& TAC)
{
    bool globalChanged = false;

    for (auto& TACFunc : TAC)
    {
        bool localChanged = true;

        while (localChanged)
        {
            localChanged = false;

            if (!TACFunc->Blocks.empty())
            {
                localChanged = WalkTACDomTree(TACFunc->Blocks[0].get(), TACFunc->getDominatorTreeInfo());
            }

            if (localChanged)
            {
                globalChanged = true;
            }
        }

        if (globalChanged)
        {
            TACFunc->getVarUsesInfo().isValid = false;
        }
    }

    return globalChanged;
}
