#include "CTFE.h"
#include "CFGBuilder.h"
#include "TACGenerator.h"
#include <climits>
#include <cstdint>
#include <string>

int Execute(TAC& TAC, TACFunction& Func, std::vector<int> args)
{
    std::unordered_map<std::string, int> varStates;

    for (size_t i = 0; i < Func.Parameters.size(); i++)
    {
        varStates[Func.Parameters[i]] = args[i];
    }

    auto getState = [&varStates](TACValue TACVal) -> int {
        if (isConstant(TACVal.value))
        {
            return stoi(TACVal.value);
        }

        else
        {
            return TACVal.neg ? -varStates[TACVal.value] : varStates[TACVal.value];
        }
    };

    size_t prevBlockID;
    TACBlock* curBlock = Func.Blocks[0].get();

    while (curBlock != nullptr)
    {
        for (auto& inst : curBlock->Instructions)
        {
            switch (inst->type)
            {
                case TACType::PHI:
                    {
                        TACPhi* phi = static_cast<TACPhi*>(inst.get());

                        int value;

                        for (PhiArgument& arg : phi->args)
                        {
                            if (arg.SourceID == prevBlockID)
                                value = getState(TACValue{arg.Value});
                        }

                        varStates[phi->variable.value] = value;
                    }
                    break;

                case TACType::ASSIGN:
                    {
                        TACAssign* assign = static_cast<TACAssign*>(inst.get());

                        varStates[assign->dest.value] = getState(assign->source);
                    }
                    break;

                case TACType::BINARYOP:
                    {
                        TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                        switch (bin->op)
                        {
                            case BinaryOp::PLUS:
                                varStates[bin->dest.value] = getState(bin->left) + getState(bin->right);
                                break;
                            case BinaryOp::MINUS:
                                varStates[bin->dest.value] = getState(bin->left) - getState(bin->right);
                                break;
                            case BinaryOp::MUL:
                                varStates[bin->dest.value] = getState(bin->left) * getState(bin->right);
                                break;
                            case BinaryOp::DIV:
                                varStates[bin->dest.value] = getState(bin->left) / getState(bin->right);
                                break;
                        }
                    }
                    break;

                case TACType::CALL:
                    {
                        TACCall* call = static_cast<TACCall*>(inst.get());

                        if (call->dest.has_value())
                        {
                            std::vector<int> intArgs;

                            for (TACValue& arg : call->args)
                            {
                                intArgs.push_back(getState(arg));
                            }

                            varStates[call->dest->value] = Execute(TAC, TAC.findFuncByName(call->functionName), intArgs);
                        }
                    }
                    break;

                case TACType::BRANCH:
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
                            curBlock = (*std::ranges::find_if(Func.Blocks, [&br](size_t ID) { return ID == br->TrueTarget; }, &TACBlock::ID)).get();
                        }

                        else
                        {
                            prevBlockID = curBlock->ID;
                            curBlock = (*std::ranges::find_if(Func.Blocks, [&br](size_t ID) { return ID == br->FalseTarget; }, &TACBlock::ID)).get();
                        }
                    }
                    break;

                case TACType::JUMP:
                    {
                        TACJump* jump = static_cast<TACJump*>(inst.get());

                        prevBlockID = curBlock->ID;

                        curBlock = (*std::ranges::find_if(Func.Blocks, [&jump](size_t ID) { return ID == jump->TargetBlock; }, &TACBlock::ID)).get();
                    }
                    break;

                case TACType::RETURN:
                    {
                        TACReturn* ret = static_cast<TACReturn*>(inst.get());

                        return getState(ret->ReturnValue);
                    }
                    break;
            }
        }
    }

    return INT_MAX;
}

void EvaluateConstantFunctions(TAC& TAC)
{
    for (TACFunction& Func : TAC)
    {
        for (auto& Block : Func.Blocks)
        {
            for (auto& inst : Block->Instructions)
            {
                switch (inst->type)
                {
                    case TACType::CALL:
                        {
                            TACCall* call = static_cast<TACCall*>(inst.get());

                            if (call->dest.has_value())
                            {
                                bool constant = true;

                                std::vector<int> intArgs;

                                int returnVal;

                                for (TACValue& arg : call->args)
                                {
                                    if (!isConstant(arg.value))
                                    {
                                        constant = false;
                                        break;
                                    }

                                    intArgs.push_back(std::stoi(arg.value));
                                }

                                if (constant)
                                {
                                    returnVal = Execute(TAC, TAC.findFuncByName(call->functionName), intArgs);

                                    auto assignInst = std::make_unique<TACAssign>();

                                    assignInst->dest = call->dest.value();
                                    assignInst->source = TACValue{std::to_string(returnVal)};

                                    inst = std::move(assignInst);
                                }
                            }
                        }
                        break;
                }
            }
        }
    }
}
