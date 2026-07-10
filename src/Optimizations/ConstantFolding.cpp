#include "ConstantFolding.h"
#include "TACGenerator.h"
#include <cctype>
#include <charconv>
#include <memory>
#include <utility>

bool isConstant(std::string_view str)
{
    int value;

    auto [ptr, err] = std::from_chars(str.data(), str.data() + str.size(), value);

    return err == std::errc{} && ptr == str.data() + str.size();
}

void FoldConstants(TAC& TAC)
{
    for (TACFunction& Function : TAC)
    {
        for (auto& block : Function.Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BINARYOP)
                {
                    TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                    if (isConstant(bin->left.value) && isConstant(bin->right.value))
                    {
                        int left = std::stoi(bin->left.value);
                        int right = std::stoi(bin->right.value);

                        int result = 0;

                        switch (bin->op)
                        {
                            case BinaryOp::PLUS:
                                result = left + right;
                                break;
                            case BinaryOp::MINUS:
                                result = left - right;
                                break;
                            case BinaryOp::MUL:
                                result = left * right;
                                break;
                            case BinaryOp::DIV:
                                result = left / right;
                                break;
                            case BinaryOp::DOUBLE_EQUAL:
                                result = left == right;
                                break;
                            case BinaryOp::NOT_EQUAL:
                                result = left != right;
                                break;
                            case BinaryOp::GREATER:
                                result = left > right;
                                break;
                            case BinaryOp::GREATER_EQUAL:
                                result = left >= right;
                                break;
                            case BinaryOp::LESS:
                                result = left < right;
                                break;
                            case BinaryOp::LESS_EQUAL:
                                result = left <= right;
                                break;
                        }

                        auto assignInst = std::make_unique<TACAssign>();

                        assignInst->dest = bin->dest;
                        assignInst->source = TACValue{std::to_string(result)};

                        inst = std::move(assignInst);
                    }
                }
            }
        }
    }
}
