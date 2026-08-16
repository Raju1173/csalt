#include <format>
#include <memory>
#include <string>
#include <variant>
#include "IREditor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "TACInstructions.h"

bool SimplifyAlgebra(TAC& TAC)
{
    bool changed = false;

    for (auto& Function : TAC)
    {
        for (auto& block : Function->Blocks)
        {
            std::unordered_map<TACValue, TACValue> negDefs;

            for (auto& inst : block->Instructions)
            {
                bool replaceWithAssign = false;
                bool replaceWithBinary = false;
                bool replaceWithNeg = false;

                TACValue result;
                std::unique_ptr<TACBinaryOp> resultBinary;
                std::string info;

                if (inst->type == TACInstType::NEG)
                {
                    auto* neg = static_cast<TACNeg*>(inst.get());

                    if (negDefs.contains(neg->source))
                    {
                        result = negDefs[neg->source];
                        info = std::format("simplified double negation of {}", TACValToStr(result));
                        replaceWithAssign = true;
                    }

                    else
                    {
                        if (auto* var = std::get_if<TACVariable>(&neg->dest))
                        {
                            negDefs[*var] = neg->source;
                        }
                    }

                    if (replaceWithAssign)
                    {
                        IREditor<TACTypes>::replaceInstruction(block.get(), inst.get(), std::make_unique<TACAssign>(neg->dest, result), Message{Phase::ALG_SIMP, IRTransformType::REPLACED, info});
                        changed = true;
                    }
                }

                else if (inst->type == TACInstType::BINARYOP)
                {
                    TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                    auto left = std::get_if<int>(&bin->left);
                    auto right = std::get_if<int>(&bin->right);

                    switch (bin->op)
                    {
                        case BinaryOp::PLUS:
                            if (left && *left == 0)
                            {
                                result = bin->right;
                                info = std::format("simplified '0 + {}' to '{}'", TACValToStr(result), TACValToStr(result));
                                replaceWithAssign = true;
                            }

                            else if (right && *right == 0)
                            {
                                result = bin->left;
                                info = std::format("simplified '{} + 0' to '{}'", TACValToStr(result), TACValToStr(result));
                                replaceWithAssign = true;
                            }

                            else if (!right && negDefs.contains(bin->right))
                            {
                                resultBinary = std::make_unique<TACBinaryOp>(bin->dest, bin->left, BinaryOp::MINUS, negDefs[bin->right]);
                                info = std::format("simplified '{} + {}(-{})' to '{} - {}'", TACValToStr(bin->left), TACValToStr(bin->right), TACValToStr(resultBinary->right), TACValToStr(resultBinary->left), TACValToStr(resultBinary->right));
                                replaceWithBinary = true;
                            }

                            else if (!left && negDefs.contains(bin->left))
                            {
                                resultBinary = std::make_unique<TACBinaryOp>(bin->dest, bin->right, BinaryOp::MINUS, negDefs[bin->left]);
                                info = std::format("simplified '{}(-{}) + {}' to '{} - {}'", TACValToStr(bin->left), TACValToStr(resultBinary->right), TACValToStr(bin->right), TACValToStr(resultBinary->left), TACValToStr(resultBinary->right));
                                replaceWithBinary = true;
                            }

                            else if (!left && !right)
                            {
                                if ((negDefs.contains(bin->left) && negDefs[bin->left] == bin->right) ||
                                    (negDefs.contains(bin->right) && negDefs[bin->right] == bin->left))
                                {
                                    result = 0;
                                    info = "simplified 'x + -x' to '0'";
                                    replaceWithAssign = true;
                                }
                            }
                            break;

                        case BinaryOp::MINUS:
                            if (right && *right == 0)
                            {
                                result = bin->left;
                                info = std::format("simplified '{} - 0' to '{}'", TACValToStr(result), TACValToStr(result));
                                replaceWithAssign = true;
                            }

                            else if (left && *left == 0 && !right)
                            {
                                result = bin->right;
                                info = std::format("simplified '0 - {}' to 'neg {}'", TACValToStr(result), TACValToStr(result));
                                replaceWithNeg = true;
                            }

                            else if (!left && !right && bin->left == bin->right)
                            {
                                result = 0;
                                info = std::format("simplified '{} - {}' to '0'", TACValToStr(bin->left), TACValToStr(bin->right));
                                replaceWithAssign = true;
                            }

                            else if (!left && negDefs.contains(bin->right))
                            {
                                resultBinary = std::make_unique<TACBinaryOp>(bin->dest, bin->left, BinaryOp::PLUS, negDefs[bin->right]);
                                info = std::format("simplified '{} - {}(-{})' to '{} + {}'", TACValToStr(bin->left), TACValToStr(bin->right), TACValToStr(resultBinary->right), TACValToStr(resultBinary->left), TACValToStr(resultBinary->right));
                                replaceWithBinary = true;
                            }
                            break;

                        case BinaryOp::MUL:
                            if ((left && *left == 0) ^ (right && *right == 0))
                            {
                                result = 0;
                                info = std::format("simplified '{} * {}' to '0'", TACValToStr(bin->left), TACValToStr(bin->right));
                                replaceWithAssign = true;
                            }

                            else if ((left && *left == 1) ^ (right && *right == 1))
                            {
                                result = (left && *left == 1) ? bin->right : bin->left;
                                info = std::format("simplified '{} * {}' to '{}'", TACValToStr(bin->left), TACValToStr(bin->right), TACValToStr(result));
                                replaceWithAssign = true;
                            }

                            else if ((left && *left == -1 && !right) || (right && *right == -1 && !left))
                            {
                                result = (left && *left == -1) ? bin->right : bin->left;
                                info = std::format("simplified '{} * {}' to 'neg {}'", TACValToStr(bin->left), TACValToStr(bin->right), TACValToStr(result));
                                replaceWithNeg = true;
                            }

                            else if (negDefs.contains(bin->left) && negDefs.contains(bin->right))
                            {
                                resultBinary = std::make_unique<TACBinaryOp>(bin->dest, negDefs[bin->left], bin->op, negDefs[bin->right]);
                                info = std::format("simplified '{}(-{}) * {}(-{})' to '{} * {}'", TACValToStr(bin->left), TACValToStr(resultBinary->left), TACValToStr(bin->right), TACValToStr(resultBinary->right), TACValToStr(resultBinary->left), TACValToStr(resultBinary->right));
                                replaceWithBinary = true;
                            }
                            break;

                        case BinaryOp::DIV:
                            if (right)
                            {
                                if (*right == 1)
                                {
                                    result = bin->left;
                                    info = std::format("simplified '{} / 1' to '{}'", TACValToStr(result), TACValToStr(result));
                                    replaceWithAssign = true;
                                }

                                else if (*right == -1 && !left)
                                {
                                    result = bin->left;
                                    info = std::format("simplified '{} / -1' to 'neg {}'", TACValToStr(result), TACValToStr(result));
                                    replaceWithNeg = true;
                                }
                            }

                            else if (left && *left == 0)
                            {
                                result = 0;
                                info = std::format("simplified '0 / {}' to '0'", TACValToStr(bin->right));
                                replaceWithAssign = true;
                            }

                            else if (!left && !right && bin->left == bin->right)
                            {
                                result = 1;
                                info = std::format("simplified '{} / {}' to '1'", TACValToStr(bin->left), TACValToStr(bin->right));
                                replaceWithAssign = true;
                            }

                            else if (negDefs.contains(bin->left) && negDefs.contains(bin->right))
                            {
                                resultBinary = std::make_unique<TACBinaryOp>(bin->dest, negDefs[bin->left], bin->op, negDefs[bin->right]);
                                info = std::format("simplified '{}(-{}) / {}(-{})' to '{} / {}'", TACValToStr(bin->left), TACValToStr(resultBinary->left), TACValToStr(bin->right), TACValToStr(resultBinary->right), TACValToStr(resultBinary->left), TACValToStr(resultBinary->right));
                                replaceWithBinary = true;
                            }
                            break;

                        case BinaryOp::GREATER:
                        case BinaryOp::GREATER_EQUAL:
                        case BinaryOp::LESS:
                        case BinaryOp::LESS_EQUAL:
                        case BinaryOp::NOT_EQUAL:
                        case BinaryOp::DOUBLE_EQUAL:
                            if (!left && !right && bin->left == bin->right)
                            {
                                result = bin->op == BinaryOp::NOT_EQUAL || bin->op == BinaryOp::GREATER || bin->op == BinaryOp::LESS ? 0 : 1;
                                info = std::format("simplified '{} {} {}' to '{}'", TACValToStr(bin->left), BinaryOpToStr(bin->op), TACValToStr(bin->right), TACValToStr(result));
                                replaceWithAssign = true;
                            }
                            break;

                        default:
                            break;
                    }

                    if (replaceWithAssign)
                    {
                        IREditor<TACTypes>::replaceInstruction(block.get(), inst.get(), std::make_unique<TACAssign>(bin->dest, result), Message{Phase::ALG_SIMP, IRTransformType::REPLACED, info});
                        changed = true;
                    }

                    else if (replaceWithBinary)
                    {
                        IREditor<TACTypes>::replaceInstruction(block.get(), inst.get(), std::move(resultBinary), Message{Phase::ALG_SIMP, IRTransformType::REPLACED, info});
                        changed = true;
                    }

                    else if (replaceWithNeg)
                    {
                        IREditor<TACTypes>::replaceInstruction(block.get(), inst.get(), std::make_unique<TACNeg>(bin->dest, result), Message{Phase::ALG_SIMP, IRTransformType::REPLACED, info});
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}
