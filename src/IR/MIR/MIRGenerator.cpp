#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include "TACGenerator.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <print>
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
