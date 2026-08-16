#include "CTFE.h"
#include "TACGenerator.h"
#include <climits>
#include <format>
#include <string>

int Execute(TAC& TAC, TACFunction* Func, std::vector<int> args)
{
    std::unordered_map<std::string, int> varStates;

    for (size_t i = 0; i < Func->Parameters.size(); i++)
    {
        varStates[Func->Parameters[i]] = args[i];
    }

    auto getState = [&varStates](TACValue TACVal) -> int {
        if (TACVal.index() == 0)
        {
            return std::get<int>(TACVal);
        }

        else
        {
            return varStates[std::get<TACVariable>(TACVal).SSAName];
        }
    };

    size_t prevBlockID;
    TACBlock* curBlock = Func->Blocks[0].get();

    while (curBlock != nullptr)
    {
        for (auto& inst : curBlock->Instructions)
        {
            switch (inst->type)
            {
                case TACInstType::PHI:
                    {
                        TACPhi* phi = static_cast<TACPhi*>(inst.get());

                        int value;

                        for (PhiArgument& arg : phi->args)
                        {
                            if (arg.SourceBlock->ID == prevBlockID)
                                value = getState(TACValue{arg.Value});
                        }

                        varStates[std::get<TACVariable>(phi->variable).SSAName] = value;
                    }
                    break;

                case TACInstType::ASSIGN:
                    {
                        TACAssign* assign = static_cast<TACAssign*>(inst.get());

                        varStates[std::get<TACVariable>(assign->dest).SSAName] = getState(assign->source);
                    }
                    break;

                case TACInstType::NEG:
                    {
                        TACNeg* neg = static_cast<TACNeg*>(inst.get());

                        varStates[std::get<TACVariable>(neg->dest).SSAName] = -getState(neg->source);
                    }
                    break;

                case TACInstType::BINARYOP:
                    {
                        TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                        switch (bin->op)
                        {
                            case BinaryOp::PLUS:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) + getState(bin->right);
                                break;
                            case BinaryOp::MINUS:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) - getState(bin->right);
                                break;
                            case BinaryOp::MUL:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) * getState(bin->right);
                                break;
                            case BinaryOp::DIV:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) / getState(bin->right);
                                break;
                            case BinaryOp::DOUBLE_EQUAL:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) == getState(bin->right);
                                break;
                            case BinaryOp::NOT_EQUAL:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) != getState(bin->right);
                                break;
                            case BinaryOp::GREATER:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) > getState(bin->right);
                                break;
                            case BinaryOp::GREATER_EQUAL:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) >= getState(bin->right);
                                break;
                            case BinaryOp::LESS:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) < getState(bin->right);
                                break;
                            case BinaryOp::LESS_EQUAL:
                                varStates[std::get<TACVariable>(bin->dest).SSAName] = getState(bin->left) <= getState(bin->right);
                                break;
                        }
                    }
                    break;

                case TACInstType::CALL:
                    {
                        TACCall* call = static_cast<TACCall*>(inst.get());

                        if (call->dest.has_value())
                        {
                            std::vector<int> intArgs;

                            for (TACValue& arg : call->args)
                            {
                                intArgs.push_back(getState(arg));
                            }

                            varStates[std::get<TACVariable>(call->dest.value()).SSAName] = Execute(TAC, std::find_if(TAC.begin(), TAC.end(), [&call](auto& func) { return func->Name == call->functionName; })->get(), intArgs);
                        }
                    }
                    break;

                case TACInstType::BRANCH:
                    {
                        TACBranch* br = static_cast<TACBranch*>(inst.get());

                        bool condition = false;

                        switch (br->cond.Op)
                        {
                            case BinaryOp::DOUBLE_EQUAL:
                                condition = getState(br->cond.Left) == getState(br->cond.Right);
                                break;
                            case BinaryOp::NOT_EQUAL:
                                condition = getState(br->cond.Left) != getState(br->cond.Right);
                                break;
                            case BinaryOp::LESS:
                                condition = getState(br->cond.Left) < getState(br->cond.Right);
                                break;
                            case BinaryOp::LESS_EQUAL:
                                condition = getState(br->cond.Left) <= getState(br->cond.Right);
                                break;
                            case BinaryOp::GREATER:
                                condition = getState(br->cond.Left) > getState(br->cond.Right);
                                break;
                            case BinaryOp::GREATER_EQUAL:
                                condition = getState(br->cond.Left) >= getState(br->cond.Right);
                                break;
                        }

                        if (condition)
                        {
                            prevBlockID = curBlock->ID;
                            curBlock = std::find_if(Func->Blocks.begin(), Func->Blocks.end(), [&br](auto& b) { return b.get() == br->TrueTarget; })->get();
                        }

                        else
                        {
                            prevBlockID = curBlock->ID;
                            curBlock = std::find_if(Func->Blocks.begin(), Func->Blocks.end(), [&br](auto& b) { return b.get() == br->FalseTarget; })->get();
                        }
                    }
                    break;

                case TACInstType::JUMP:
                    {
                        TACJump* jump = static_cast<TACJump*>(inst.get());

                        prevBlockID = curBlock->ID;

                        curBlock = std::find_if(Func->Blocks.begin(), Func->Blocks.end(), [&jump](auto& b) { return b.get() == jump->TargetBlock; })->get();
                    }
                    break;

                case TACInstType::RETURN:
                    {
                        TACReturn* ret = static_cast<TACReturn*>(inst.get());

                        return getState(ret->ReturnValue.value());
                    }
                    break;
            }
        }
    }

    return INT_MAX;
}

void EvaluateConstantFunctions(TAC& TAC)
{
    for (auto& Func : TAC)
    {
        for (auto& Block : Func->Blocks)
        {
            for (auto& inst : Block->Instructions)
            {
                if (inst->type == TACInstType::CALL)
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    if (call->dest.has_value())
                    {
                        bool constant = true;

                        std::vector<int> intArgs;

                        int returnVal;

                        for (TACValue& arg : call->args)
                        {
                            if (arg.index() != 0)
                            {
                                constant = false;
                                break;
                            }

                            intArgs.push_back(std::get<int>(arg));
                        }

                        if (constant)
                        {
                            returnVal = Execute(TAC, std::find_if(TAC.begin(), TAC.end(), [&call](auto& func) { return func->Name == call->functionName; })->get(), intArgs);

                            std::string argsString = "(";

                            for (size_t i = 0; i < call->args.size(); i++)
                            {
                                argsString += std::format("{}", TACValToStr(call->args[i]));

                                if (i + 1 != call->args.size())
                                    argsString += ", ";
                            }

                            argsString += ")";

                            IREditor<TACTypes>::replaceInstruction(Block.get(), inst.get(), std::make_unique<TACAssign>(call->dest.value(), returnVal), Message{Phase::CTFE, IRTransformType::REPLACED, std::format("evaluated function call with compile time constant arguments '{}{}' to {}", call->functionName, argsString, returnVal)});
                        }
                    }
                }
            }
        }
    }
}
