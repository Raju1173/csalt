#include "ASMGenerator.h"
#include <print>

std::vector<std::unique_ptr<MIRFunction>> GenerateMachineIR(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    std::vector<std::unique_ptr<MIRFunction>> MIR;

    for (auto& TACFunc : TAC)
    {
        MIR.push_back(std::make_unique<MIRFunction>(TACFunc->Name, TACFunc->Parameters));

        for (auto& TACBlock : TACFunc->Blocks)
        {
            MIR.back()->Blocks.push_back(std::make_unique<MIRBlock>(TACBlock->ID));

            MIRBlock* curBlock = MIR.back()->Blocks.back().get();

            for (auto& inst : TACBlock->Instructions)
            {
                if (auto assign = dynamic_cast<TACAssign*>(inst.get()))
                {
                    std::print("    {} = {}\n", assign->dest.value, assign->source.value);
                }

                else if (auto binary = dynamic_cast<TACBinaryOp*>(inst.get()))
                {
                    std::string op;

                    switch (binary->op)
                    {
                        case BinaryOp::PLUS:
                            op = "+";
                            break;
                        case BinaryOp::MINUS:
                            op = "-";
                            break;
                        case BinaryOp::MUL:
                            op = "*";
                            break;
                        case BinaryOp::DIV:
                            op = "/";
                            break;
                        case BinaryOp::DOUBLE_EQUAL:
                            op = "==";
                            break;
                        case BinaryOp::NOT_EQUAL:
                            op = "!=";
                            break;
                        case BinaryOp::LESS:
                            op = "<";
                            break;
                        case BinaryOp::LESS_EQUAL:
                            op = "<=";
                            break;
                        case BinaryOp::GREATER:
                            op = ">";
                            break;
                        case BinaryOp::GREATER_EQUAL:
                            op = ">=";
                            break;
                    }

                    std::print("    {} = {} {} {}\n", binary->dest.value, binary->left.value, op, binary->right.value);
                }

                else if (auto call = dynamic_cast<TACCall*>(inst.get()))
                {
                    if (call->dest.has_value())
                        std::print("    {} = ", call->dest->value);
                    else
                        std::print("    ");

                    std::print("call {}(", call->functionName);

                    for (size_t i = 0; i < call->args.size(); i++)
                    {
                        std::print("{}", call->args[i].value);

                        if (i + 1 != call->args.size())
                            std::print(", ");
                    }

                    std::print(")\n");
                }

                else if (auto branch = dynamic_cast<TACBranch*>(inst.get()))
                {
                    std::print("    branch {} ? B{} : B{}\n", branch->Condition.value, branch->TrueTarget, branch->FalseTarget);
                }

                else if (auto jump = dynamic_cast<TACJump*>(inst.get()))
                {
                    std::print("    jump B{}\n", jump->TargetBlock);
                }

                else if (auto ret = dynamic_cast<TACReturn*>(inst.get()))
                {
                    if (ret->ReturnValue.value.empty())
                        std::print("    return\n");
                    else
                        std::print("    return {}\n", ret->ReturnValue.value);
                }
            }
        }
    }

    return MIR;
}
