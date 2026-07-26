#include "ConstantFolding.h"
#include "TACGenerator.h"
#include "TACEditor.h"
#include <cctype>
#include <format>
#include <memory>
#include <print>
#include <utility>

bool FoldConstants(TAC& TAC)
{
    bool changed = false;

    for (auto& Function : TAC)
    {
        for (auto& block : Function->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BINARYOP)
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
                        }

                        Message message = Message{TACPass::CONSTANT_FOLDING, TACTransformType::REPLACED, std::format("simplified {} {} {} to {}", *left, BinaryOpToStr[std::to_underlying(bin->op)], *right, result)};

                        TACEditor::replaceInstruction(block.get(), inst.get(), std::make_unique<TACAssign>(bin->dest, result), message);

                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}
