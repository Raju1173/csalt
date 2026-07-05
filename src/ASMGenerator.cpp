#include "ASMGenerator.h"
#include "TACGenerator.h"
#include <cctype>
#include <cstddef>
#include <memory>
#include <print>
#include <utility>
#include <variant>
#include <fstream>

std::map<std::string, std::variant<Register, StackOffset>> StackSlots;

int NextOffset = -4;

Operand GetOperand(const TACValue& TACVal, std::unique_ptr<MIRFunction>& CurFunc)
{
    if (TACVal.value[0] >= '0' && TACVal.value[0] <= '9')
        return Immediate{std::stoi(TACVal.value)};

    if (StackSlots.contains(TACVal.value))
        return std::visit([](auto&& arg) -> Operand { return arg; }, StackSlots[TACVal.value]);

    StackSlots[TACVal.value] = StackOffset{NextOffset};

    NextOffset -= 4;

    CurFunc->StackFrameSize += 4;

    return std::visit([](auto&& arg) -> Operand { return arg; }, StackSlots[TACVal.value]);
}

constexpr Register ArgRegs[] = {
    Register::EDI,
    Register::ESI,
    Register::EDX,
    Register::ECX,
    Register::R8D,
    Register::R9D};

std::vector<std::unique_ptr<MIRFunction>> GenerateMachineIR(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    std::vector<std::unique_ptr<MIRFunction>> MIR;

    for (auto& TACFunc : TAC)
    {
        StackSlots.clear();


        MIR.push_back(std::make_unique<MIRFunction>(TACFunc->Name, TACFunc->Parameters));

        MIR.back()->StackFrameSize += 4;
        int TempOffset = -4;
        NextOffset = -8;

        for (auto& TACBlock : TACFunc->Blocks)
        {
            MIR.back()->Blocks.push_back(std::make_unique<MIRBlock>(TACBlock->ID));

            MIRBlock* curBlock = MIR.back()->Blocks.back().get();

            for (auto& inst : TACBlock->Instructions)
            {
                if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
                {
                    auto movSrc = std::make_unique<MIRMov>();

                    movSrc->Dest = Register::EAX;
                    movSrc->Source = GetOperand(assign->source, MIR.back());

                    curBlock->Instructions.push_back(std::move(movSrc));

                    auto movDest = std::make_unique<MIRMov>();

                    movDest->Dest = GetOperand(assign->dest, MIR.back());
                    movDest->Source = Register::EAX;

                    curBlock->Instructions.push_back(std::move(movDest));
                }

                else if (auto binary = dynamic_cast<TACBinaryOp*>(inst.get()))
                {
                    if (binary->op == BinaryOp::PLUS || binary->op == BinaryOp::MINUS || binary->op == BinaryOp::MUL)
                    {
                        auto movOperand = std::make_unique<MIRMov>();

                        movOperand->Dest = Register::EAX;
                        movOperand->Source = GetOperand(binary->left, MIR.back());

                        curBlock->Instructions.push_back(std::move(movOperand));

                        switch (binary->op)
                        {
                            case BinaryOp::PLUS:
                                {
                                    auto operation = std::make_unique<MIRAdd>();

                                    operation->Dest = Register::EAX;
                                    operation->Source = GetOperand(binary->right, MIR.back());

                                    curBlock->Instructions.push_back(std::move(operation));
                                }
                                break;

                            case BinaryOp::MINUS:
                                {
                                    auto operation = std::make_unique<MIRSub>();

                                    operation->Dest = Register::EAX;
                                    operation->Source = GetOperand(binary->right, MIR.back());

                                    curBlock->Instructions.push_back(std::move(operation));
                                }
                                break;

                            case BinaryOp::MUL:
                                {
                                    auto operation = std::make_unique<MIRImul>();

                                    operation->Dest = Register::EAX;
                                    operation->Source = GetOperand(binary->right, MIR.back());

                                    curBlock->Instructions.push_back(std::move(operation));
                                }
                                break;
                        }

                        auto movResult = std::make_unique<MIRMov>();

                        movResult->Dest = GetOperand(binary->dest, MIR.back());
                        movResult->Source = Register::EAX;

                        curBlock->Instructions.push_back(std::move(movResult));
                    }

                    else
                    {
                        auto movDividend = std::make_unique<MIRMov>();

                        movDividend->Dest = Register::EAX;
                        movDividend->Source = GetOperand(binary->left, MIR.back());

                        curBlock->Instructions.push_back(std::move(movDividend));

                        curBlock->Instructions.push_back(std::make_unique<MIRCdq>());

                        Operand divisor = GetOperand(binary->right, MIR.back());

                        if (divisor.index() == 2)
                        {
                            auto movDivisor = std::make_unique<MIRMov>();

                            movDivisor->Dest = StackOffset{TempOffset};
                            movDivisor->Source = divisor;

                            curBlock->Instructions.push_back(std::move(movDivisor));

                            auto idiv = std::make_unique<MIRIdiv>();

                            idiv->Divisor = StackOffset{TempOffset};

                            curBlock->Instructions.push_back(std::move(idiv));
                        }

                        else
                        {
                            auto idiv = std::make_unique<MIRIdiv>();

                            idiv->Divisor = divisor;

                            curBlock->Instructions.push_back(std::move(idiv));
                        }

                        auto idivResult = std::make_unique<MIRMov>();

                        idivResult->Dest = GetOperand(binary->dest, MIR.back());
                        idivResult->Source = Register::EAX;

                        curBlock->Instructions.push_back(std::move(idivResult));
                    }
                }

                else if (auto call = dynamic_cast<TACCall*>(inst.get()))
                {
                    for (size_t i = 0; i < call->args.size(); i++)
                    {
                        if (i < 6)
                        {
                            auto movArgument = std::make_unique<MIRMov>();

                            movArgument->Dest = ArgRegs[i];

                            movArgument->Source = GetOperand(call->args[i], MIR.back());

                            curBlock->Instructions.push_back(std::move(movArgument));
                        }

                        else
                        {
                            auto pushArg = std::make_unique<MIRPush>();

                            pushArg->Source = GetOperand(call->args[call->args.size() - 1 - (i - 6)], MIR.back());

                            curBlock->Instructions.push_back(std::move(pushArg));
                        }
                    }

                    auto MIRcall = std::make_unique<MIRCall>();

                    MIRcall->Function = call->functionName;

                    curBlock->Instructions.push_back(std::move(MIRcall));

                    if (call->dest.has_value())
                    {
                        auto movReturnVal = std::make_unique<MIRMov>();

                        movReturnVal->Dest = GetOperand(call->dest.value(), MIR.back());
                        movReturnVal->Source = Register::EAX;

                        curBlock->Instructions.push_back(std::move(movReturnVal));
                    }

                    auto add = std::make_unique<MIRAdd>();

                    add->Dest = Register::RSP;
                    add->Source = Immediate{std::max(0, static_cast<int>(call->args.size()) - 6) * 4};

                    curBlock->Instructions.push_back(std::move(add));
                }

                else if (auto branch = dynamic_cast<TACBranch*>(inst.get()))
                {
                    auto CMP = std::make_unique<MIRCmp>();

                    CMP->Left = GetOperand(branch->cond.Left, MIR.back());
                    CMP->Right = GetOperand(branch->cond.Right, MIR.back());

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

                    condJump->TargetBlock = branch->TrueTarget;

                    curBlock->Instructions.push_back(std::move(condJump));

                    auto jump = std::make_unique<MIRJump>();

                    jump->TargetBlock = branch->FalseTarget;

                    curBlock->Instructions.push_back(std::move(jump));
                }

                else if (auto jump = dynamic_cast<TACJump*>(inst.get()))
                {
                    auto jmp = std::make_unique<MIRJump>();

                    jmp->TargetBlock = jump->TargetBlock;

                    curBlock->Instructions.push_back(std::move(jmp));
                }

                else if (auto ret = dynamic_cast<TACReturn*>(inst.get()))
                {
                    if (!ret->ReturnValue.value.empty())
                    {
                        auto movRetVal = std::make_unique<MIRMov>();

                        movRetVal->Dest = Register::EAX;
                        movRetVal->Source = GetOperand(ret->ReturnValue, MIR.back());

                        curBlock->Instructions.push_back(std::move(movRetVal));
                    }

                    auto movRBP = std::make_unique<MIRMov>();

                    movRBP->Dest = Register::RSP;
                    movRBP->Source = Register::RBP;

                    curBlock->Instructions.push_back(std::move(movRBP));

                    auto popRBP = std::make_unique<MIRPop>();

                    popRBP->Dest = Register::RBP;

                    curBlock->Instructions.push_back(std::move(popRBP));

                    curBlock->Instructions.push_back(std::make_unique<MIRRet>());
                }
            }
        }

        auto& exitBlock = MIR.back()->Blocks.back();

        if (exitBlock->Instructions.empty() || !dynamic_cast<MIRRet*>(exitBlock->Instructions.back().get()))
        {
            auto movRBP = std::make_unique<MIRMov>();

            movRBP->Dest = Register::RSP;
            movRBP->Source = Register::RBP;

            exitBlock->Instructions.push_back(std::move(movRBP));

            auto popRBP = std::make_unique<MIRPop>();

            popRBP->Dest = Register::RBP;

            exitBlock->Instructions.push_back(std::move(popRBP));

            exitBlock->Instructions.push_back(std::make_unique<MIRRet>());
        }

        auto& entryBlock = MIR.back()->Blocks[0];

        std::vector<std::unique_ptr<MIRInstruction>> MovParameterInstructions;

        for (size_t i = 0; i < MIR.back()->Parameters.size(); i++)
        {
            if (i < 6)
            {
                auto movParameter = std::make_unique<MIRMov>();

                movParameter->Dest = GetOperand(TACValue{MIR.back()->Parameters[i] + "0"}, MIR.back());

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

                movParameter->Dest = GetOperand(TACValue{MIR.back()->Parameters[i] + "0"}, MIR.back());

                movParameter->Source = Register::EAX;

                MovParameterInstructions.push_back(std::move(movParameter));
            }
        }

        entryBlock->Instructions.insert(entryBlock->Instructions.begin(), std::make_move_iterator(MovParameterInstructions.begin()), std::make_move_iterator(MovParameterInstructions.end()));

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
        subFrameSize->Source = Immediate{MIR.back()->StackFrameSize};

        PrologueInstructions.push_back(std::move(subFrameSize));

        entryBlock->Instructions.insert(entryBlock->Instructions.begin(), std::make_move_iterator(PrologueInstructions.begin()), std::make_move_iterator(PrologueInstructions.end()));
    }

    return MIR;
}

static std::string RegisterName(Register reg)
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

static std::string OperandString(const Operand& op)
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
                return "DWORD PTR [rbp+" + std::to_string(value.Offset) + "]";

            return "DWORD PTR [rbp]";
        }
        else
        {
            return std::to_string(value.Value);
        }
    },
        op);
}

void EmitAssembly(const std::vector<std::unique_ptr<MIRFunction>>& MIR, const std::string& filename)
{
    std::ofstream out(filename);

    out << ".intel_syntax noprefix\n";
    out << ".text\n\n";

    for (const auto& function : MIR)
    {
        if (function->FunctionName == "main")
            out << ".global main\n";

        out << function->FunctionName << ":\n";

        for (const auto& block : function->Blocks)
        {
            out << "." << function->FunctionName << "L" << block->ID << ":\n";

            for (const auto& inst : block->Instructions)
            {
                if (auto mov = dynamic_cast<MIRMov*>(inst.get()))
                {
                    out << "\tmov " << OperandString(mov->Dest) << ", " << OperandString(mov->Source) << "\n";
                }

                else if (auto add = dynamic_cast<MIRAdd*>(inst.get()))
                {
                    out << "\tadd " << OperandString(add->Dest) << ", " << OperandString(add->Source) << "\n";
                }

                else if (auto sub = dynamic_cast<MIRSub*>(inst.get()))
                {
                    out << "\tsub " << OperandString(sub->Dest) << ", " << OperandString(sub->Source) << "\n";
                }

                else if (auto mul = dynamic_cast<MIRImul*>(inst.get()))
                {
                    out << "\timul " << OperandString(mul->Dest) << ", " << OperandString(mul->Source) << "\n";
                }

                else if (auto div = dynamic_cast<MIRIdiv*>(inst.get()))
                {
                    out << "\tidiv " << OperandString(div->Divisor) << "\n";
                }

                else if (auto cmp = dynamic_cast<MIRCmp*>(inst.get()))
                {
                    out << "\tcmp " << OperandString(cmp->Left) << ", " << OperandString(cmp->Right) << "\n";
                }

                else if (auto push = dynamic_cast<MIRPush*>(inst.get()))
                {
                    out << "\tpush " << OperandString(push->Source) << "\n";
                }

                else if (auto pop = dynamic_cast<MIRPop*>(inst.get()))
                {
                    out << "\tpop " << OperandString(pop->Dest) << "\n";
                }

                else if (auto jump = dynamic_cast<MIRJump*>(inst.get()))
                {
                    out << "\tjmp ." << function->FunctionName << "L" << jump->TargetBlock << "\n";
                }

                else if (auto jump = dynamic_cast<MIRCondJump*>(inst.get()))
                {
                    out << "\t";

                    switch (jump->Cond)
                    {
                        case Condition::EQUAL:
                            out << "je ";
                            break;
                        case Condition::NOT_EQUAL:
                            out << "jne ";
                            break;
                        case Condition::LESS:
                            out << "jl ";
                            break;
                        case Condition::LESS_EQUAL:
                            out << "jle ";
                            break;
                        case Condition::GREATER:
                            out << "jg ";
                            break;
                        case Condition::GREATER_EQUAL:
                            out << "jge ";
                            break;
                    }

                    out << "." << function->FunctionName << "L" << jump->TargetBlock << "\n";
                }

                else if (auto call = dynamic_cast<MIRCall*>(inst.get()))
                {
                    out << "\tcall " << call->Function << "\n";
                }

                else if (dynamic_cast<MIRRet*>(inst.get()))
                {
                    out << "\tret\n";
                }

                else if (dynamic_cast<MIRCdq*>(inst.get()))
                {
                    out << "\tcdq\n";
                }
            }

            out << "\n";
        }

        out << "\n";
    }

    out << ".section .note.GNU-stack, \"\", @progbits\n";
}


void EmitExecutable(std::string ASMFilePath, std::string ExecFilePath)
{
    std::string cmd = "gcc " + ASMFilePath + " -o " + ExecFilePath;

    std::system(cmd.c_str());
}

void PrintOutput(std::string ExecFilePath)
{
    std::string cmd = "./" + ExecFilePath + "; echo 'EXIT CODE : ' $?";

    std::system(cmd.c_str());
}
