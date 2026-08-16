#include "ConstantFolding.h"
#include "TACGenerator.h"
#include <cctype>
#include <format>
#include <memory>
#include <print>

bool FoldConstants(TAC& TAC)
{
    bool changed = false;

    for (auto& Function : TAC)
    {
        for (auto& block : Function->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACInstType::BINARYOP)
                {
                    TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                    auto* left = std::get_if<int>(&bin->left);
                    auto* right = std::get_if<int>(&bin->right);

                    if (left && right)
                    {
                        int result = 0;

                        switch (bin->op)
                        {
                            case BinaryOp::PLUS:
                                result = *left + *right;
                                break;
                            case BinaryOp::MINUS:
                                result = *left - *right;
                                break;
                            case BinaryOp::MUL:
                                result = *left * *right;
                                break;
                            case BinaryOp::DIV:
                                result = *left / *right;
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

                        Message message = Message{Phase::FOLDING, IRTransformType::REPLACED, std::format("simplified {} {} {} to {}", *left, BinaryOpToStr(bin->op), *right, result)};

                        IREditor<TACTypes>::replaceInstruction(block.get(), inst.get(), std::make_unique<TACAssign>(bin->dest, result), message);

                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}
