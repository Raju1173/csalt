#include "MIRGenerator.h"
#include "TACGenerator.h"
#include <cctype>
#include <cstddef>
#include <memory>
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

                    case TACType::BINARYOP:
                        {
                            auto binary = dynamic_cast<TACBinaryOp*>(inst.get());

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

                    case TACType::JUMP:
                        {
                            TACJump* jump = dynamic_cast<TACJump*>(inst.get());

                            auto jmp = std::make_unique<MIRJump>();

                            jmp->TargetBlock = jump->TargetBlock;

                            curBlock.Instructions.push_back(std::move(jmp));
                        }

                    case TACType::RETURN:
                        {
                            TACReturn* ret = dynamic_cast<TACReturn*>(inst.get());

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
                }
            }
        }

        auto& exitBlock = MIR.back().Blocks.back();

        if (exitBlock.Instructions.empty() || !dynamic_cast<MIRRet*>(exitBlock.Instructions.back().get()))
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

void PrintMIR(MIR& MIR)
{
    //
}
