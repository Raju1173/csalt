#include "MIRGenerator.h"
#include "TACGenerator.h"
#include <cctype>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <print>
#include <sys/types.h>
#include <utility>
#include <variant>

std::unordered_map<std::string, std::variant<Register, StackOffset>> StackSlots;

int NextOffset = -4;

Operand GetOperand(const TACValue& TACVal, MIRFunction& CurFunc)
{
    if ((TACVal.value[0] >= '0' && TACVal.value[0] <= '9') || TACVal.value[0] == '-')
        return Immediate{std::stoi(TACVal.value)};

    if (StackSlots.contains(TACVal.value))
        return std::visit([](auto&& arg) -> Operand { return arg; }, StackSlots[TACVal.value]);

    StackSlots[TACVal.value] = StackOffset{NextOffset};

    NextOffset -= 4;

    CurFunc.StackFrameSize += 4;

    return std::visit([](auto&& arg) -> Operand { return arg; }, StackSlots[TACVal.value]);
}

constexpr Register ArgRegs[] = {
    Register::EDI,
    Register::ESI,
    Register::EDX,
    Register::ECX,
    Register::R8D,
    Register::R9D};

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
                            TACCall* call = static_cast<TACCall*>(inst.get());

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
                                    auto pushArg = std::make_unique<MIRPush>();

                                    pushArg->Source = GetOperand(call->args[call->args.size() - 1 - (i - 6)], MIR.back());

                                    curBlock.Instructions.push_back(std::move(pushArg));
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

                            auto add = std::make_unique<MIRAdd>();

                            add->Dest = Register::RSP;
                            add->Source = Immediate{std::max(0, static_cast<int>(call->args.size()) - 6) * 4};

                            curBlock.Instructions.push_back(std::move(add));
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
                                CMP->Left = left;
                                CMP->Right = right;
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

                movParameter->Dest = GetOperand(TACValue{MIR.back().Parameters[i] + "0"}, MIR.back());

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

std::string OperandString(const Operand& op)
{
    return std::visit([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Register>)
        {
            return RegisterName(value);
        }

        else if constexpr (std::is_same_v<T, StackOffset>)
        {
            if (value.Offset < 0)
                return "DWORD PTR [rbp" + std::to_string(value.Offset) + "]";

            if (value.Offset > 0)
                return "DWORD PTR [rbp + " + std::to_string(value.Offset) + "]";

            return "DWORD PTR [rbp]";
        }

        else
        {
            return std::to_string(value.Value);
        }
    },
        op);
}

void PrintMIR(MIR& MIR)
{
    for (const MIRFunction& function : MIR)
    {
        std::print("# Function - {}:\n\n", function.FunctionName);

        for (const auto& block : function.Blocks)
        {
            std::print("B{}:\n", block.ID);

            for (const auto& inst : block.Instructions)
            {
                switch (inst->type)
                {
                    case MIRType::MOV:
                        {
                            MIRMov* mov = static_cast<MIRMov*>(inst.get());

                            std::print("    mov {}, {}\n", OperandString(mov->Dest), OperandString(mov->Source));
                        }
                        break;

                    case MIRType::ADD:
                        {
                            MIRAdd* add = static_cast<MIRAdd*>(inst.get());

                            std::print("    add {}, {}\n", OperandString(add->Dest), OperandString(add->Source));
                        }
                        break;

                    case MIRType::SUB:
                        {
                            MIRSub* sub = static_cast<MIRSub*>(inst.get());

                            std::print("    sub {}, {}\n", OperandString(sub->Dest), OperandString(sub->Source));
                        }
                        break;

                    case MIRType::MUL:
                        {
                            MIRImul* mul = static_cast<MIRImul*>(inst.get());

                            std::print("    imul {}, {}\n", OperandString(mul->Dest), OperandString(mul->Source));
                        }
                        break;

                    case MIRType::DIV:
                        {
                            MIRIdiv* div = static_cast<MIRIdiv*>(inst.get());

                            std::print("    idiv {}\n", OperandString(div->Divisor));
                        }
                        break;

                    case MIRType::NEG:
                        {
                            MIRNeg* neg = static_cast<MIRNeg*>(inst.get());

                            std::print("    neg {}\n", OperandString(neg->Dest));
                        }
                        break;

                    case MIRType::CMP:
                        {
                            MIRCmp* cmp = static_cast<MIRCmp*>(inst.get());

                            std::print("    cmp {}, {}\n", OperandString(cmp->Left), OperandString(cmp->Right));
                        }
                        break;

                    case MIRType::PUSH:
                        {
                            MIRPush* push = static_cast<MIRPush*>(inst.get());

                            std::print("    push {}\n", OperandString(push->Source));
                        }
                        break;

                    case MIRType::POP:
                        {
                            MIRPop* pop = static_cast<MIRPop*>(inst.get());

                            std::print("    pop {}\n", OperandString(pop->Dest));
                        }
                        break;

                    case MIRType::JMP:
                        {
                            MIRJump* jump = static_cast<MIRJump*>(inst.get());

                            std::print("    jmp B{}\n", jump->TargetBlock);
                        }
                        break;

                    case MIRType::CJMP:
                        {
                            MIRCondJump* jump = static_cast<MIRCondJump*>(inst.get());

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

                            std::print("B{}\n", jump->TargetBlock);
                        }
                        break;

                    case MIRType::CALL:
                        {
                            MIRCall* call = static_cast<MIRCall*>(inst.get());

                            std::print("    call {}\n", call->Function);
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
            }

            std::print("\n");
        }

        std::print("\n");
    }
}
