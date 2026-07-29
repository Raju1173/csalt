#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include "TACGenerator.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <print>
#include <string>
#include <sys/types.h>
#include <variant>
#include <ranges>

std::unordered_map<TACValue, Operand> VarLocations;

int NextOffset = -4;
int NextVirtualReg = 0;

struct PendingOutgoingArg
{
    MIRMov* Instruction;
    int ArgIndex;
};

constexpr Register ArgRegs[] = {
    Register::EDI,
    Register::ESI,
    Register::EDX,
    Register::ECX,
    Register::R8D,
    Register::R9D,
};

Operand GetOperand(TACValue& TACVal)
{
    if (std::holds_alternative<int>(TACVal))
        return Immediate{std::get<int>(TACVal)};

    if (VarLocations.contains(TACVal))
        return VarLocations[TACVal];

    VarLocations[TACVal] = VirtualRegister{NextVirtualReg++};

    return VarLocations[TACVal];
}

MIR GenerateMachineIR(TAC& TAC)
{
    MIR MIR;

    for (auto& TACFunc : TAC)
    {
        MIR.push_back(std::make_unique<MIRFunction>(TACFunc->Name, TACFunc->Parameters));

        auto& curFunc = MIR.back();

        int MaxOutgoingArgs = 0;
        std::vector<PendingOutgoingArg> PendingOutgoingArgs;

        std::unordered_map<TACBlock*, MIRBlock*> blockMap;

        for (auto& TACBlock : std::views::reverse(TACFunc->Blocks))
        {
            curFunc->Blocks.push_back(std::make_unique<MIRBlock>(TACBlock->ID, MIR.back().get()));
            blockMap[TACBlock.get()] = MIR.back()->Blocks.back().get();
        }

        for (auto& TACBlock : std::views::reverse(TACFunc->Blocks))
        {
            MIR.back()->Blocks.push_back(std::make_unique<MIRBlock>(TACBlock->ID, MIR.back().get()));

            MIRBlock* curBlock = blockMap[TACBlock.get()];

            for (auto& inst : TACBlock->Instructions)
            {
                // not using switch to reduce nesting and keep indentation simpler...
                if (inst->type == TACType::ASSIGN)
                {
                    TACAssign* assign = static_cast<TACAssign*>(inst.get());

                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(assign->dest), GetOperand(assign->source)));
                }

                if (inst->type == TACType::NEG)
                {
                    TACNeg* neg = static_cast<TACNeg*>(inst.get());

                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(neg->dest), GetOperand(neg->source)));

                    curBlock->Instructions.push_back(std::make_unique<MIRNeg>(GetOperand(neg->dest)));
                }

                if (inst->type == TACType::BINARYOP)
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    Operand dest = GetOperand(binary->dest);
                    Operand left = GetOperand(binary->left);
                    Operand right = GetOperand(binary->right);

                    if (binary->op == BinaryOp::PLUS || binary->op == BinaryOp::MINUS || binary->op == BinaryOp::MUL)
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, left));

                        if (binary->op == BinaryOp::PLUS)
                            curBlock->Instructions.push_back(std::make_unique<MIRAdd>(dest, right));

                        else if (binary->op == BinaryOp::MINUS)
                            curBlock->Instructions.push_back(std::make_unique<MIRSub>(dest, right));

                        else if (binary->op == BinaryOp::MUL)
                            curBlock->Instructions.push_back(std::make_unique<MIRImul>(dest, right));
                    }

                    else
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));

                        curBlock->Instructions.push_back(std::make_unique<MIRCdq>());

                        Operand divisor = right;

                        if (divisor.index() == 3)
                        {
                            VirtualRegister tempReg{NextVirtualReg++};

                            curBlock->Instructions.push_back(std::make_unique<MIRMov>(tempReg, divisor));

                            divisor = tempReg;
                        }

                        curBlock->Instructions.push_back(std::make_unique<MIRIdiv>(divisor));

                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(dest, Register::EAX));
                        break;
                    }
                }

                if (inst->type == TACType::CALL)
                {
                    MIR.back()->IsLeaf = false;

                    TACCall* call = static_cast<TACCall*>(inst.get());

                    int extraArgs = call->args.size() - 6;

                    if (extraArgs > MaxOutgoingArgs)
                        MaxOutgoingArgs = extraArgs;

                    for (size_t i = 0; i < call->args.size(); i++)
                    {
                        if (i < 6)
                        {
                            curBlock->Instructions.push_back(std::make_unique<MIRMov>(ArgRegs[i], GetOperand(call->args[i])));
                        }

                        else
                        {
                            auto movArgument = std::make_unique<MIRMov>();
                            movArgument->Source = GetOperand(call->args[i]);
                            PendingOutgoingArgs.push_back({movArgument.get(), static_cast<int>(i - 6)});
                            curBlock->Instructions.push_back(std::move(movArgument));
                        }
                    }

                    curBlock->Instructions.push_back(std::make_unique<MIRCall>(std::find_if(MIR.begin(), MIR.end(), [&call](auto& func) { return func->FunctionName == call->functionName; })->get()));

                    if (call->dest.has_value())
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(GetOperand(call->dest.value()), Register::EAX));
                    }
                }

                if (inst->type == TACType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    auto CMP = std::make_unique<MIRCmp>();
                    Operand left = GetOperand(branch->cond.Left);
                    Operand right = GetOperand(branch->cond.Right);

                    if ((std::holds_alternative<StackOffset>(right) && std::holds_alternative<StackOffset>(left)) || (std::holds_alternative<Immediate>(right) && std::holds_alternative<Immediate>(left)))
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, left));

                        CMP->Left = Register::EAX;
                        CMP->Right = right;
                    }

                    else
                    {
                        if (std::holds_alternative<Immediate>(left))
                        {
                            CMP->Left = left;
                            CMP->Right = right;
                        }

                        else
                        {
                            CMP->Left = right;
                            CMP->Right = left;
                        }
                    }

                    curBlock->Instructions.push_back(std::move(CMP));

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
                            condJump->Cond = Condition::LESS;
                            break;
                        case BinaryOp::LESS_EQUAL:
                            condJump->Cond = Condition::LESS_EQUAL;
                            break;
                        case BinaryOp::GREATER:
                            condJump->Cond = Condition::GREATER;
                            break;
                        case BinaryOp::GREATER_EQUAL:
                            condJump->Cond = Condition::GREATER_EQUAL;
                            break;
                    }

                    condJump->TargetBlock = blockMap[branch->TrueTarget];
                    curBlock->Instructions.push_back(std::move(condJump));

                    curBlock->Instructions.push_back(std::make_unique<MIRJump>(blockMap[branch->FalseTarget]));
                }

                if (inst->type == TACType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(inst.get());

                    curBlock->Instructions.push_back(std::make_unique<MIRJump>(blockMap[jump->TargetBlock]));
                }

                if (inst->type == TACType::RETURN)
                {
                    TACReturn* ret = static_cast<TACReturn*>(inst.get());

                    if (!ret->ReturnValue.has_value())
                    {
                        curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::EAX, GetOperand(ret->ReturnValue.value())));
                    }

                    curBlock->Instructions.push_back(std::make_unique<MIRMov>(Register::RSP, Register::RBP));

                    curBlock->Instructions.push_back(std::make_unique<MIRPop>(Register::RBP));
                    curBlock->Instructions.push_back(std::make_unique<MIRRet>());
                }
            }
        }
    }

    return MIR;
}

/*
MIR GenerateMachineIR(TAC& TAC)
{
    MIR MIR;

    for (TACFunction& TACFunc : TAC)
    {
        StackSlots.clear();

        MIR.push_back(MIRFunction{TACFunc.Name, TACFunc.Parameters});

        MIR.back().StackFrameSize += 4;
        int TempOffset = -4;
        NextOffset = -8;

        int MaxOutgoingArgs = 0;
        std::vector<PendingOutgoingArg> PendingOutgoingArgs;

        for (auto& TACBlock : TACFunc.Blocks)
        {
            MIR.back().Blocks.push_back(MIRBlock{TACBlock->ID});

            MIRBlock& curBlock = MIR.back().Blocks.back();

            for (auto& inst : TACBlock->Instructions)
            {
                switch (inst->type)
                {
                    case TACType::ASSIGN:
                        {
                            TACAssign* assign = static_cast<TACAssign*>(inst.get());

                            auto movSrc = std::make_unique<MIRMov>();

                            movSrc->Dest = Register::EAX;
                            movSrc->Source = GetOperand(assign->source, MIR.back());

                            curBlock.Instructions.push_back(std::move(movSrc));

                            if (assign->source.neg)
                            {
                                auto negSrc = std::make_unique<MIRNeg>();

                                negSrc->Dest = Register::EAX;

                                curBlock.Instructions.push_back(std::move(negSrc));
                            }

                            auto movDest = std::make_unique<MIRMov>();

                            movDest->Dest = GetOperand(assign->dest, MIR.back());
                            movDest->Source = Register::EAX;

                            curBlock.Instructions.push_back(std::move(movDest));
                        }
                        break;

                    case TACType::BINARYOP:
                        {
                            TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                            if (binary->op == BinaryOp::PLUS || binary->op == BinaryOp::MINUS || binary->op == BinaryOp::MUL)
                            {
                                auto movOperand = std::make_unique<MIRMov>();

                                movOperand->Dest = Register::EAX;
                                movOperand->Source = GetOperand(binary->left, MIR.back());

                                curBlock.Instructions.push_back(std::move(movOperand));

                                if (binary->left.neg)
                                {
                                    auto negLeft = std::make_unique<MIRNeg>();
                                    negLeft->Dest = Register::EAX;
                                    curBlock.Instructions.push_back(std::move(negLeft));
                                }

                                switch (binary->op)
                                {
                                    case BinaryOp::PLUS:
                                        {
                                            if (binary->right.neg)
                                            {
                                                auto operation = std::make_unique<MIRSub>();

                                                operation->Dest = Register::EAX;
                                                operation->Source = GetOperand(binary->right, MIR.back());

                                                curBlock.Instructions.push_back(std::move(operation));
                                            }

                                            else
                                            {
                                                auto operation = std::make_unique<MIRAdd>();

                                                operation->Dest = Register::EAX;
                                                operation->Source = GetOperand(binary->right, MIR.back());

                                                curBlock.Instructions.push_back(std::move(operation));
                                            }
                                        }
                                        break;

                                    case BinaryOp::MINUS:
                                        {
                                            if (binary->right.neg)
                                            {
                                                auto operation = std::make_unique<MIRAdd>();

                                                operation->Dest = Register::EAX;
                                                operation->Source = GetOperand(binary->right, MIR.back());

                                                curBlock.Instructions.push_back(std::move(operation));
                                            }

                                            else
                                            {
                                                auto operation = std::make_unique<MIRSub>();

                                                operation->Dest = Register::EAX;
                                                operation->Source = GetOperand(binary->right, MIR.back());

                                                curBlock.Instructions.push_back(std::move(operation));
                                            }
                                        }
                                        break;

                                    case BinaryOp::MUL:
                                        {
                                            auto operation = std::make_unique<MIRImul>();

                                            operation->Dest = Register::EAX;
                                            operation->Source = GetOperand(binary->right, MIR.back());

                                            curBlock.Instructions.push_back(std::move(operation));

                                            if (binary->right.neg)
                                            {
                                                auto negResult = std::make_unique<MIRNeg>();
                                                negResult->Dest = Register::EAX;
                                                curBlock.Instructions.push_back(std::move(negResult));
                                            }
                                        }
                                        break;
                                }

                                auto movResult = std::make_unique<MIRMov>();

                                movResult->Dest = GetOperand(binary->dest, MIR.back());
                                movResult->Source = Register::EAX;

                                curBlock.Instructions.push_back(std::move(movResult));
                            }

                            else
                            {
                                auto movDividend = std::make_unique<MIRMov>();

                                movDividend->Dest = Register::EAX;
                                movDividend->Source = GetOperand(binary->left, MIR.back());

                                curBlock.Instructions.push_back(std::move(movDividend));

                                if (binary->left.neg)
                                {
                                    auto negLeft = std::make_unique<MIRNeg>();
                                    negLeft->Dest = Register::EAX;
                                    curBlock.Instructions.push_back(std::move(negLeft));
                                }

                                curBlock.Instructions.push_back(std::make_unique<MIRCdq>());

                                Operand divisor = GetOperand(binary->right, MIR.back());

                                if (divisor.index() == 2)
                                {
                                    auto movDivisor = std::make_unique<MIRMov>();

                                    movDivisor->Dest = StackOffset{TempOffset};
                                    movDivisor->Source = divisor;

                                    curBlock.Instructions.push_back(std::move(movDivisor));

                                    auto idiv = std::make_unique<MIRIdiv>();

                                    idiv->Divisor = StackOffset{TempOffset};

                                    curBlock.Instructions.push_back(std::move(idiv));
                                }

                                else
                                {
                                    auto idiv = std::make_unique<MIRIdiv>();

                                    idiv->Divisor = divisor;

                                    curBlock.Instructions.push_back(std::move(idiv));
                                }

                                if (binary->right.neg)
                                {
                                    auto negResult = std::make_unique<MIRNeg>();
                                    negResult->Dest = Register::EAX;
                                    curBlock.Instructions.push_back(std::move(negResult));
                                }

                                auto idivResult = std::make_unique<MIRMov>();

                                idivResult->Dest = GetOperand(binary->dest, MIR.back());
                                idivResult->Source = Register::EAX;

                                curBlock.Instructions.push_back(std::move(idivResult));
                            }
                        }
                        break;

                    case TACType::CALL:
                        {
                            MIR.back().IsLeaf = false;

                            TACCall* call = static_cast<TACCall*>(inst.get());

                            int extraArgs = call->args.size() - 6;

                            if (extraArgs > MaxOutgoingArgs)
                                MaxOutgoingArgs = extraArgs;

                            for (size_t i = 0; i < call->args.size(); i++)
                            {
                                if (i < 6)
                                {
                                    auto movArgument = std::make_unique<MIRMov>();

                                    movArgument->Dest = ArgRegs[i];
                                    movArgument->Source = GetOperand(call->args[i], MIR.back());

                                    curBlock.Instructions.push_back(std::move(movArgument));
                                }

                                else
                                {
                                    auto movArgument = std::make_unique<MIRMov>();

                                    movArgument->Source = GetOperand(call->args[i], MIR.back());

                                    PendingOutgoingArgs.push_back({movArgument.get(), static_cast<int>(i - 6)});

                                    curBlock.Instructions.push_back(std::move(movArgument));
                                }
                            }

                            auto MIRcall = std::make_unique<MIRCall>();

                            MIRcall->Function = call->functionName;

                            curBlock.Instructions.push_back(std::move(MIRcall));

                            if (call->dest.has_value())
                            {
                                auto movReturnVal = std::make_unique<MIRMov>();

                                movReturnVal->Dest = GetOperand(call->dest.value(), MIR.back());
                                movReturnVal->Source = Register::EAX;

                                curBlock.Instructions.push_back(std::move(movReturnVal));
                            }
                        }
                        break;

                    case TACType::BRANCH:
                        {
                            TACBranch* branch = static_cast<TACBranch*>(inst.get());

                            auto CMP = std::make_unique<MIRCmp>();


                            auto left = GetOperand(branch->cond.Left, MIR.back());
                            auto right = GetOperand(branch->cond.Right, MIR.back());

                            if ((right.index() == 1 && left.index() == 1) || (right.index() == 2 && left.index() == 2))
                            {
                                auto movLeft = std::make_unique<MIRMov>();

                                movLeft->Dest = Register::EAX;
                                movLeft->Source = left;

                                curBlock.Instructions.push_back(std::move(movLeft));

                                CMP->Left = Register::EAX;
                                CMP->Right = right;
                            }

                            else
                            {
                                if (left.index() != 2)
                                {
                                    CMP->Left = left;
                                    CMP->Right = right;
                                }

                                else
                                {
                                    CMP->Left = right;
                                    CMP->Right = left;
                                }
                            }

                            curBlock.Instructions.push_back(std::move(CMP));

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
                                    condJump->Cond = Condition::LESS;
                                    break;
                                case BinaryOp::LESS_EQUAL:
                                    condJump->Cond = Condition::LESS_EQUAL;
                                    break;
                                case BinaryOp::GREATER:
                                    condJump->Cond = Condition::GREATER;
                                    break;
                                case BinaryOp::GREATER_EQUAL:
                                    condJump->Cond = Condition::GREATER_EQUAL;
                                    break;
                            }

                            condJump->TargetBlock = branch->TrueTarget;

                            curBlock.Instructions.push_back(std::move(condJump));

                            auto jump = std::make_unique<MIRJump>();

                            jump->TargetBlock = branch->FalseTarget;

                            curBlock.Instructions.push_back(std::move(jump));
                        }
                        break;

                    case TACType::JUMP:
                        {
                            TACJump* jump = static_cast<TACJump*>(inst.get());

                            auto jmp = std::make_unique<MIRJump>();

                            jmp->TargetBlock = jump->TargetBlock;

                            curBlock.Instructions.push_back(std::move(jmp));
                        }
                        break;

                    case TACType::RETURN:
                        {
                            TACReturn* ret = static_cast<TACReturn*>(inst.get());

                            if (!ret->ReturnValue.value.empty())
                            {
                                auto movRetVal = std::make_unique<MIRMov>();

                                movRetVal->Dest = Register::EAX;
                                movRetVal->Source = GetOperand(ret->ReturnValue, MIR.back());

                                curBlock.Instructions.push_back(std::move(movRetVal));
                            }

                            auto movRBP = std::make_unique<MIRMov>();

                            movRBP->Dest = Register::RSP;
                            movRBP->Source = Register::RBP;

                            curBlock.Instructions.push_back(std::move(movRBP));

                            auto popRBP = std::make_unique<MIRPop>();

                            popRBP->Dest = Register::RBP;

                            curBlock.Instructions.push_back(std::move(popRBP));

                            curBlock.Instructions.push_back(std::make_unique<MIRRet>());
                        }
                        break;
                }
            }
        }

        auto& exitBlock = MIR.back().Blocks.back();

        if (exitBlock.Instructions.empty() || exitBlock.Instructions.back()->type != MIRType::RET)
        {
            auto movRBP = std::make_unique<MIRMov>();

            movRBP->Dest = Register::RSP;
            movRBP->Source = Register::RBP;

            exitBlock.Instructions.push_back(std::move(movRBP));

            auto popRBP = std::make_unique<MIRPop>();

            popRBP->Dest = Register::RBP;

            exitBlock.Instructions.push_back(std::move(popRBP));

            exitBlock.Instructions.push_back(std::make_unique<MIRRet>());
        }

        auto& entryBlock = MIR.back().Blocks[0];

        std::vector<std::unique_ptr<MIRInstruction>> MovParameterInstructions;

        for (size_t i = 0; i < MIR.back().Parameters.size(); i++)
        {
            if (i < 6)
            {
                auto movParameter = std::make_unique<MIRMov>();

                movParameter->Dest = GetOperand(TACValue{MIR.back().Parameters[i]}, MIR.back());

                movParameter->Source = ArgRegs[i];

                MovParameterInstructions.push_back(std::move(movParameter));
            }

            else
            {
                int offset = 16 + 8 * (i - 6);

                auto movArgument = std::make_unique<MIRMov>();

                movArgument->Dest = Register::EAX;

                movArgument->Source = StackOffset{offset};

                MovParameterInstructions.push_back(std::move(movArgument));

                auto movParameter = std::make_unique<MIRMov>();

                movParameter->Dest = GetOperand(TACValue{MIR.back().Parameters[i] + "0"}, MIR.back());

                movParameter->Source = Register::EAX;

                MovParameterInstructions.push_back(std::move(movParameter));
            }
        }

        entryBlock.Instructions.insert(entryBlock.Instructions.begin(), std::make_move_iterator(MovParameterInstructions.begin()), std::make_move_iterator(MovParameterInstructions.end()));

        if (MaxOutgoingArgs > 0)
        {
            MIR.back().StackFrameSize += MaxOutgoingArgs * 8;

            for (auto& pending : PendingOutgoingArgs)
            {
                int offset = -MIR.back().StackFrameSize + pending.ArgIndex * 8;
                pending.Instruction->Dest = StackOffset{offset};
            }
        }

        std::vector<std::unique_ptr<MIRInstruction>> PrologueInstructions;

        auto pushRBP = std::make_unique<MIRPush>();

        pushRBP->Source = Register::RBP;

        PrologueInstructions.push_back(std::move(pushRBP));

        auto movRSP = std::make_unique<MIRMov>();

        movRSP->Dest = Register::RBP;
        movRSP->Source = Register::RSP;

        PrologueInstructions.push_back(std::move(movRSP));

        auto subFrameSize = std::make_unique<MIRSub>();

        subFrameSize->Dest = Register::RSP;
        subFrameSize->Source = Immediate{MIR.back().StackFrameSize};

        PrologueInstructions.push_back(std::move(subFrameSize));

        entryBlock.Instructions.insert(entryBlock.Instructions.begin(), std::make_move_iterator(PrologueInstructions.begin()), std::make_move_iterator(PrologueInstructions.end()));
    }

    return MIR;
}
*/

std::string RegisterName(Register reg)
{
    switch (reg)
    {
        case Register::EAX:
            return "eax";

        case Register::EDI:
            return "edi";
        case Register::ESI:
            return "esi";
        case Register::EDX:
            return "edx";
        case Register::ECX:
            return "ecx";
        case Register::R8D:
            return "r8d";
        case Register::R9D:
            return "r9d";

        case Register::R10D:
            return "r10d";
        case Register::R11D:
            return "r11d";

        case Register::EBX:
            return "ebx";
        case Register::R12D:
            return "r12d";
        case Register::R13D:
            return "r13d";
        case Register::R14D:
            return "r14d";
        case Register::R15D:
            return "r15d";

        case Register::RSP:
            return "rsp";
        case Register::RBP:
            return "rbp";
    }

    return "";
}

std::string OperandString(const Operand& op, bool UseRSP)
{
    if (std::holds_alternative<Register>(op))
    {
        return RegisterName(std::get<Register>(op));
    }

    else if (std::holds_alternative<VirtualRegister>(op))
    {
        return "vreg." + std::to_string(std::get<VirtualRegister>(op).ID);
    }

    else if (std::holds_alternative<StackOffset>(op))
    {
        std::string base = UseRSP ? "rsp" : "rbp";

        if (std::get<StackOffset>(op).Offset < 0)
            return "DWORD PTR [" + base + std::to_string(std::get<StackOffset>(op).Offset) + "]";
        if (std::get<StackOffset>(op).Offset > 0)
            return "DWORD PTR [" + base + " + " + std::to_string(std::get<StackOffset>(op).Offset) + "]";

        return "DWORD PTR [" + base + "]";
    }

    else
    {
        return std::to_string(std::get<Immediate>(op).Value);
    }
}

void PrintMIRInstruction(MIRFunction* function, MIRInstruction* inst, bool history)
{
    switch (inst->type)
    {
        case MIRType::MOV:
            {
                MIRMov* mov = static_cast<MIRMov*>(inst);

                std::print("    mov {}, {}\n", OperandString(mov->Dest, function->OmitFramePtr), OperandString(mov->Source, function->OmitFramePtr));
            }
            break;

        case MIRType::ADD:
            {
                MIRAdd* add = static_cast<MIRAdd*>(inst);

                std::print("    add {}, {}\n", OperandString(add->Dest, function->OmitFramePtr), OperandString(add->Source, function->OmitFramePtr));
            }
            break;

        case MIRType::SUB:
            {
                MIRSub* sub = static_cast<MIRSub*>(inst);

                std::print("    sub {}, {}\n", OperandString(sub->Dest, function->OmitFramePtr), OperandString(sub->Source, function->OmitFramePtr));
            }
            break;

        case MIRType::MUL:
            {
                MIRImul* mul = static_cast<MIRImul*>(inst);

                std::print("    imul {}, {}\n", OperandString(mul->Dest, function->OmitFramePtr), OperandString(mul->Source, function->OmitFramePtr));
            }
            break;

        case MIRType::DIV:
            {
                MIRIdiv* div = static_cast<MIRIdiv*>(inst);

                std::print("    idiv {}\n", OperandString(div->Divisor, function->OmitFramePtr));
            }
            break;

        case MIRType::NEG:
            {
                MIRNeg* neg = static_cast<MIRNeg*>(inst);

                std::print("    neg {}\n", OperandString(neg->Dest, function->OmitFramePtr));
            }
            break;

        case MIRType::CMP:
            {
                MIRCmp* cmp = static_cast<MIRCmp*>(inst);

                std::print("    cmp {}, {}\n", OperandString(cmp->Left, function->OmitFramePtr), OperandString(cmp->Right, function->OmitFramePtr));
            }
            break;

        case MIRType::PUSH:
            {
                MIRPush* push = static_cast<MIRPush*>(inst);

                std::print("    push {}\n", OperandString(push->Source, function->OmitFramePtr));
            }
            break;

        case MIRType::POP:
            {
                MIRPop* pop = static_cast<MIRPop*>(inst);

                std::print("    pop {}\n", OperandString(pop->Dest, function->OmitFramePtr));
            }
            break;

        case MIRType::JMP:
            {
                MIRJump* jump = static_cast<MIRJump*>(inst);

                std::print("    jmp B{}\n", jump->TargetBlock->ID);
            }
            break;

        case MIRType::CJMP:
            {
                MIRCondJump* jump = static_cast<MIRCondJump*>(inst);

                std::print("    ");

                switch (jump->Cond)
                {
                    case Condition::EQUAL:
                        std::print("je ");
                        break;
                    case Condition::NOT_EQUAL:
                        std::print("jne ");
                        break;
                    case Condition::LESS:
                        std::print("jl ");
                        break;
                    case Condition::LESS_EQUAL:
                        std::print("jle ");
                        break;
                    case Condition::GREATER:
                        std::print("jg ");
                        break;
                    case Condition::GREATER_EQUAL:
                        std::print("jge ");
                        break;
                }

                std::print("B{}\n", jump->TargetBlock->ID);
            }
            break;

        case MIRType::CALL:
            {
                MIRCall* call = static_cast<MIRCall*>(inst);

                std::print("    call {}\n", call->Function->FunctionName);
            }
            break;

        case MIRType::RET:
            {
                std::print("    ret\n");
            }
            break;

        case MIRType::CDQ:
            {
                std::print("    cdq\n");
            }
            break;
    }

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : inst->History)
        {
            PrintMessage(message);
        }
    }
}

void PrintMIRBlock(MIRBlock* block, bool history)
{
    auto blockDead = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    std::print("Block - {} :\n", block->ID);

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : block->History)
        {
            PrintMessage(message);
        }
    }

    if (history)
    {
        if (block->LastDeadInstruction != nullptr)
        {
            for (auto& prec : block->LastDeadInstruction->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintMIRInstruction(block->Function, prec.get(), history);
            }

            std::print("\033[2;37m");

            PrintMIRInstruction(block->Function, block->LastDeadInstruction.get(), history);

            for (auto& trail : block->LastDeadInstruction->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintMIRInstruction(block->Function, trail.get(), history);
            }
        }
    }

    for (auto& inst : block->Instructions)
    {
        if (history)
        {
            for (auto& prec : inst->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintMIRInstruction(block->Function, prec.get(), history);
            }
        }

        if (blockDead == block->Function->Blocks.end())
            std::print("\033[2;37m");

        PrintMIRInstruction(block->Function, inst.get(), history);

        if (history)
        {
            for (auto& trail : inst->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintMIRInstruction(block->Function, trail.get(), history);
            }
        }
    }

    std::print("\n");
}

void PrintMIRFunction(MIRFunction* func, bool history)
{
    std::print("# Function - {}(", func->FunctionName);

    for (size_t j = 0; j < func->Parameters.size(); ++j)
    {
        if (j != 0)
            std::print(", ");

        std::print("{}", func->Parameters[j]);
    }

    std::print(") :\n");

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : func->History)
        {
            PrintMessage(message);
        }
    }

    if (history)
    {
        if (func->LastDeadBlock != nullptr)
        {
            for (auto& prec : func->LastDeadBlock->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintMIRBlock(prec.get(), history);
            }

            std::print("\033[2;37m");

            PrintMIRBlock(func->LastDeadBlock.get(), history);

            for (auto& trail : func->LastDeadBlock->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintMIRBlock(trail.get(), history);
            }

            std::print("\n\033[2;37mend {}\033[0m\n\n", func->FunctionName);

            return;
        }
    }

    for (auto& block : func->Blocks)
    {
        if (history)
        {
            for (auto& prec : block->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintMIRBlock(prec.get(), history);
            }
        }

        PrintMIRBlock(block.get(), history);

        if (history)
        {
            for (auto& trail : block->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintMIRBlock(trail.get(), history);
            }
        }
    }
}

void PrintMIR(MIR& MIR, bool history)
{
    if (!history)
        std::print("-----------MIR-----------\n\n");
    else
        std::print("-------MIR-HISTORY-------\n\n");

    for (auto& func : MIR)
    {
        if (history)
        {
            for (auto& prec : func->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintMIRFunction(prec.get(), history);
            }
        }

        PrintMIRFunction(func.get(), history);

        if (history)
        {
            for (auto& trail : func->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintMIRFunction(trail.get(), history);
            }
        }
    }

    std::print("\n");
}
