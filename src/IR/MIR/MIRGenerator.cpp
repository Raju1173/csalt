#include "MIRGenerator.h"
#include "Globals.h"
#include "IRDebugger.h"
#include "MIRInstructions.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <print>
#include <sys/types.h>
#include <variant>
#include <vector>
#include <unordered_map>

Register ArgRegs[] = {
    Register::EDI,
    Register::ESI,
    Register::EDX,
    Register::ECX,
    Register::R8D,
    Register::R9D,
};

void GenerateMachineIR(TAC& TAC, MIR& MIR)
{
    Debugger::AddIR(MIR);

    std::unordered_map<TACValue, Operand> VarLocations;
    int NextVirtualReg = 0;

    auto GetOperand = [&NextVirtualReg, &VarLocations](TACValue& TACVal) -> Operand {
        if (std::holds_alternative<int>(TACVal))
            return Immediate{std::get<int>(TACVal)};

        if (VarLocations.contains(TACVal))
            return VarLocations[TACVal];

        VarLocations[TACVal] = VirtualRegister{NextVirtualReg++};
        return VarLocations[TACVal];
    };

    for (auto& TACFunc : TAC)
    {
        VarLocations.clear();
        NextVirtualReg = 0;

        MIR.push_back(std::make_unique<MIRFunction>(TACFunc->Name, TACFunc->Parameters));
        auto& curFunc = MIR.back();

        curFunc->StackFrameSize = 0;
        int MaxOutgoingArgs = 0;

        std::unordered_map<TACBlock*, MIRBlock*> blockMap;

        for (auto& TACBlock : TACFunc->Blocks)
        {
            curFunc->Blocks.push_back(std::make_unique<MIRBlock>(TACBlock->ID, curFunc.get()));
            blockMap[TACBlock.get()] = curFunc->Blocks.back().get();
        }

        for (auto& TACBlock : TACFunc->Blocks)
        {
            MIRBlock* curBlock = blockMap[TACBlock.get()];

            for (size_t i = 0; i < TACBlock->Instructions.size(); i++)
            {
                auto& inst = TACBlock->Instructions[i];

                if (inst->type == TACInstType::ASSIGN)
                {
                    TACAssign* assign = static_cast<TACAssign*>(inst.get());
                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(assign->dest), GetOperand(assign->source)));
                }

                else if (inst->type == TACInstType::NEG)
                {
                    TACNeg* neg = static_cast<TACNeg*>(inst.get());
                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(neg->dest), GetOperand(neg->source)));
                    curBlock->Instructions.push_back(std::make_unique<MIRNeg>(GetOperand(neg->dest)));
                }

                else if (inst->type == TACInstType::BINARYOP)
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    Operand dest = GetOperand(binary->dest);
                    Operand left = GetOperand(binary->left);
                    Operand right = GetOperand(binary->right);

                    if (binary->op == BinaryOp::PLUS)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, left));
                        curBlock->Instructions.push_back(std::make_unique<MIRAdd>(dest, right));
                    }

                    else if (binary->op == BinaryOp::MINUS)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, left));
                        curBlock->Instructions.push_back(std::make_unique<MIRSub>(dest, right));
                    }

                    else if (binary->op == BinaryOp::MUL)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, left));
                        curBlock->Instructions.push_back(std::make_unique<MIRImul>(dest, right));
                    }

                    else if (binary->op == BinaryOp::DIV)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));
                        curBlock->Instructions.push_back(std::make_unique<MIRCdq>());

                        Operand divisor = right;

                        if (std::holds_alternative<Immediate>(divisor))
                        {
                            VirtualRegister tempVReg{NextVirtualReg++};
                            curBlock->Instructions.push_back(std::make_unique<MIRMov>(tempVReg, divisor));
                            divisor = tempVReg;
                        }

                        curBlock->Instructions.push_back(std::make_unique<MIRIdiv>(divisor));
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, Register::EAX));
                    }

                    else
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::R10D, Immediate{0}));

                        bool swapped = false;

                        if (std::holds_alternative<Immediate>(right) && std::holds_alternative<Immediate>(left))
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(Register::EAX, right));
                        }

                        else if (std::holds_alternative<Immediate>(left))
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(right, left));
                            swapped = true;
                        }

                        else
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(left, right));
                        }

                        switch (binary->op)
                        {
                            case BinaryOp::DOUBLE_EQUAL:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(Condition::EQUAL, Register::R10B));
                                break;
                            case BinaryOp::NOT_EQUAL:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(Condition::NOT_EQUAL, Register::R10B));
                                break;
                            case BinaryOp::LESS:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(swapped ? Condition::GREATER : Condition::LESS, Register::R10B));
                                break;
                            case BinaryOp::LESS_EQUAL:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(swapped ? Condition::GREATER_EQUAL : Condition::LESS_EQUAL, Register::R10B));
                                break;
                            case BinaryOp::GREATER:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(swapped ? Condition::LESS : Condition::GREATER, Register::R10B));
                                break;
                            case BinaryOp::GREATER_EQUAL:
                                curBlock->Instructions.push_back(std::make_unique<MIRSet>(swapped ? Condition::LESS_EQUAL : Condition::GREATER_EQUAL, Register::R10B));
                                break;
                        }

                        curBlock->Instructions.push_back(std::make_unique<MIRMovzx>(dest, Register::R10B));
                    }
                }

                else if (inst->type == TACInstType::CALL)
                {
                    curFunc->IsLeaf = false;

                    TACCall* call = static_cast<TACCall*>(inst.get());

                    int extraArgs = call->args.size() > 6 ? call->args.size() - 6 : 0;

                    if (extraArgs > MaxOutgoingArgs)
                    {
                        MaxOutgoingArgs = extraArgs;
                    }

                    for (size_t i = 0; i < std::min(call->args.size(), static_cast<size_t>(6)); i++)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(ArgRegs[i], GetOperand(call->args[i])));
                    }

                    for (size_t i = 6; i < call->args.size(); i++)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(StackOffset{.Offset = static_cast<int>((i - 6) * 8), .RSP = true}, GetOperand(call->args[i])));
                    }

                    auto funcIt = std::find_if(MIR.begin(), MIR.end(), [&call](auto& func) { return func->FunctionName == call->functionName; });

                    if (call->isSiblingCall)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::RSP, Register::RBP));
                        curBlock->Instructions.push_back(std::make_unique<MIRPop>(Register::RBP));
                        curBlock->Instructions.push_back(std::make_unique<MIRJump>(std::find_if(MIR.begin(), MIR.end(), [&call](auto& func) { return call->functionName == func->FunctionName; })->get()));

                        i++;

                        continue;
                    }

                    if (funcIt != MIR.end())
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRCall>(funcIt->get()));
                    }

                    if (call->dest.has_value())
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(call->dest.value()), Register::EAX));
                    }
                }

                else if (inst->type == TACInstType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    Operand left = GetOperand(branch->cond.Left);
                    Operand right = GetOperand(branch->cond.Right);

                    auto leftInt = std::get_if<Immediate>(&left);
                    auto rightInt = std::get_if<Immediate>(&right);

                    if (branch->cond.Op == BinaryOp::NOT_EQUAL && ((leftInt && leftInt->Value == 0) || (rightInt && rightInt->Value == 0)))
                    {
                        if (rightInt)
                            curBlock->Instructions.push_back(std::make_unique<MIRTest>(left, left));
                        else
                            curBlock->Instructions.push_back(std::make_unique<MIRTest>(right, right));

                        curBlock->Instructions.push_back(std::make_unique<MIRCondJump>(Condition::NOT_EQUAL, blockMap[branch->TrueTarget]));
                        curBlock->Instructions.push_back(std::make_unique<MIRJump>(blockMap[branch->FalseTarget]));
                        break;
                    }


                    bool swapped = false;

                    if (std::holds_alternative<Immediate>(right) && std::holds_alternative<Immediate>(left))
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));
                        curBlock->Instructions.push_back(std::make_unique<MIRCmp>(Register::EAX, right));
                    }

                    else if (std::holds_alternative<Immediate>(left))
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRCmp>(right, left));
                        swapped = true;
                    }

                    else
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRCmp>(left, right));
                    }


                    auto condJump = std::make_unique<MIRCondJump>();

                    switch (branch->cond.Op)
                    {
                        case BinaryOp::DOUBLE_EQUAL:
                            condJump->Cond = Condition::EQUAL;
                            break;
                        case BinaryOp::NOT_EQUAL:
                            condJump->Cond = Condition::NOT_EQUAL;
                            break;
                        case BinaryOp::LESS:
                            condJump->Cond = swapped ? Condition::GREATER : Condition::LESS;
                            break;
                        case BinaryOp::LESS_EQUAL:
                            condJump->Cond = swapped ? Condition::GREATER_EQUAL : Condition::LESS_EQUAL;
                            break;
                        case BinaryOp::GREATER:
                            condJump->Cond = swapped ? Condition::LESS : Condition::GREATER;
                            break;
                        case BinaryOp::GREATER_EQUAL:
                            condJump->Cond = swapped ? Condition::LESS_EQUAL : Condition::GREATER_EQUAL;
                            break;
                    }

                    condJump->TargetBlock = blockMap[branch->TrueTarget];
                    curBlock->Instructions.push_back(std::move(condJump));
                    curBlock->Instructions.push_back(std::make_unique<MIRJump>(blockMap[branch->FalseTarget]));
                }

                else if (inst->type == TACInstType::SELECT)
                {
                    TACSelect* select = static_cast<TACSelect*>(inst.get());

                    Operand dest = GetOperand(select->dest);
                    Operand trueVal = GetOperand(select->TrueVal);
                    Operand falseVal = GetOperand(select->FalseVal);

                    Operand left = GetOperand(select->cond.Left);
                    Operand right = GetOperand(select->cond.Right);

                    auto leftInt = std::get_if<Immediate>(&left);
                    auto rightInt = std::get_if<Immediate>(&right);

                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, falseVal));

                    bool swapped = false;

                    if (select->cond.Op == BinaryOp::NOT_EQUAL && ((leftInt && leftInt->Value == 0) || (rightInt && rightInt->Value == 0)))
                    {
                        if (rightInt)
                            curBlock->Instructions.push_back(std::make_unique<MIRTest>(left, left));
                        else
                            curBlock->Instructions.push_back(std::make_unique<MIRTest>(right, right));
                    }

                    else
                    {
                        if (std::holds_alternative<Immediate>(right) && std::holds_alternative<Immediate>(left))
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(Register::EAX, right));
                        }

                        else if (std::holds_alternative<Immediate>(left))
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(right, left));
                            swapped = true;
                        }

                        else
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRCmp>(left, right));
                        }
                    }

                    Condition condition;

                    switch (select->cond.Op)
                    {
                        case BinaryOp::DOUBLE_EQUAL:
                            condition = Condition::EQUAL;
                            break;
                        case BinaryOp::NOT_EQUAL:
                            condition = Condition::NOT_EQUAL;
                            break;
                        case BinaryOp::LESS:
                            condition = swapped ? Condition::GREATER : Condition::LESS;
                            break;
                        case BinaryOp::LESS_EQUAL:
                            condition = swapped ? Condition::GREATER_EQUAL : Condition::LESS_EQUAL;
                            break;
                        case BinaryOp::GREATER:
                            condition = swapped ? Condition::LESS : Condition::GREATER;
                            break;
                        case BinaryOp::GREATER_EQUAL:
                            condition = swapped ? Condition::LESS_EQUAL : Condition::GREATER_EQUAL;
                            break;
                    }

                    curBlock->Instructions.push_back(std::make_unique<MIRCmov>(condition, dest, trueVal));
                }

                else if (inst->type == TACInstType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(inst.get());
                    curBlock->Instructions.push_back(std::make_unique<MIRJump>(blockMap[jump->TargetBlock]));
                }

                else if (inst->type == TACInstType::RETURN)
                {
                    TACReturn* ret = static_cast<TACReturn*>(inst.get());

                    if (ret->ReturnValue.has_value())
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, GetOperand(ret->ReturnValue.value())));
                    }

                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::RSP, Register::RBP));
                    curBlock->Instructions.push_back(std::make_unique<MIRPop>(Register::RBP));
                    curBlock->Instructions.push_back(std::make_unique<MIRRet>());
                }
            }
        }

        auto& exitBlock = curFunc->Blocks.back();

        MIRInstruction* lastInst = !exitBlock->Instructions.empty() ? exitBlock->Instructions.back().get() : nullptr;

        if (lastInst == nullptr || (lastInst->type != MIRInstType::RET && lastInst->type != MIRInstType::JMP && !std::holds_alternative<MIRFunction*>(static_cast<MIRJump*>(lastInst)->Target)))
        {
            exitBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, Immediate{0}));
            exitBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::RSP, Register::RBP));
            exitBlock->Instructions.push_back(std::make_unique<MIRPop>(Register::RBP));
            exitBlock->Instructions.push_back(std::make_unique<MIRRet>());
        }

        auto& entryBlock = curFunc->Blocks.front();

        std::vector<std::unique_ptr<MIRInstruction>> ParameterMovInstructions;

        for (size_t i = 0; i < curFunc->Parameters.size(); i++)
        {
            TACValue paramTACVal = TACVariable{curFunc->Parameters[i]};

            if (i < 6)
            {
                ParameterMovInstructions.push_back(std::make_unique<MIRMov>(GetOperand(paramTACVal), ArgRegs[i]));
            }

            else
            {
                int offset = 16 + ((i - 6) * 8);
                ParameterMovInstructions.push_back(std::make_unique<MIRMov>(Register::EAX, StackOffset{offset}));
                ParameterMovInstructions.push_back(std::make_unique<MIRMov>(GetOperand(paramTACVal), Register::EAX));
            }
        }

        entryBlock->Instructions.insert(entryBlock->Instructions.begin(), std::make_move_iterator(ParameterMovInstructions.begin()), std::make_move_iterator(ParameterMovInstructions.end()));

        std::vector<std::unique_ptr<MIRInstruction>> PrologueInstructions;

        PrologueInstructions.push_back(std::make_unique<MIRPush>(Register::RBP));
        PrologueInstructions.push_back(std::make_unique<MIRMov>(Register::RBP, Register::RSP));

        curFunc->StackFrameSize += (MaxOutgoingArgs * 8);

        PrologueInstructions.push_back(std::make_unique<MIRSub>(Register::RSP, Immediate{curFunc->StackFrameSize}));

        entryBlock->Instructions.insert(entryBlock->Instructions.begin(), std::make_move_iterator(PrologueInstructions.begin()), std::make_move_iterator(PrologueInstructions.end()));

        MIR.back()->NextBlockID = MIR.back()->Blocks.size() + 1;
    }

    Debugger::Notify(Phase::MIR_CONST);
}
