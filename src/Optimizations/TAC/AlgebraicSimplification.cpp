#include <memory>
#include "TACGenerator.h"
#include "ConstantFolding.h"

bool SimplifyAlgebra(TAC& TAC)
{
    bool changed = false;

    for (TACFunction& Function : TAC)
    {
        for (auto& block : Function.Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BINARYOP)
                {
                    TACBinaryOp* bin = static_cast<TACBinaryOp*>(inst.get());

                    bool leftConst = isConstant(bin->left.value);
                    bool rightConst = isConstant(bin->right.value);

                    bool replaceInst = false;
                    TACValue result;

                    switch (bin->op)
                    {
                        case BinaryOp::PLUS:
                            if (leftConst && std::stoi(bin->left.value) == 0)
                            {
                                result = bin->right;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (rightConst && std::stoi(bin->right.value) == 0)
                            {
                                result = bin->left;
                                replaceInst = true;
                                changed = true;
                            }
                            break;

                        case BinaryOp::MINUS:
                            if (rightConst && std::stoi(bin->right.value) == 0)
                            {
                                result = bin->left;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (leftConst && std::stoi(bin->left.value) == 0 && !rightConst)
                            {
                                result = bin->right;
                                result.neg = !result.neg;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (leftConst && rightConst && bin->left.value == bin->right.value && bin->left.neg == bin->right.neg)
                            {
                                result = TACValue{"0"};
                                replaceInst = true;
                                changed = true;
                            }
                            break;

                        case BinaryOp::MUL:
                            if ((leftConst && std::stoi(bin->left.value) == 0) || (rightConst && std::stoi(bin->right.value) == 0))
                            {
                                result = TACValue{"0"};
                                replaceInst = true;
                                changed = true;
                            }

                            else if (leftConst && std::stoi(bin->left.value) == 1)
                            {
                                result = bin->right;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (rightConst && std::stoi(bin->right.value) == 1)
                            {
                                result = bin->left;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (leftConst && std::stoi(bin->left.value) == -1 && !rightConst)
                            {
                                result = bin->right;
                                result.neg = !result.neg;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (rightConst && std::stoi(bin->right.value) == -1 && !leftConst)
                            {
                                result = bin->left;
                                result.neg = !result.neg;
                                replaceInst = true;
                                changed = true;
                            }

                            else if (bin->left.neg && bin->right.neg)
                            {
                                bin->left.neg = false;
                                bin->right.neg = false;
                                changed = true;
                            }
                            break;

                        case BinaryOp::DIV:
                            if (rightConst)
                            {
                                if (std::stoi(bin->right.value) == 1)
                                {
                                    result = bin->left;
                                    replaceInst = true;
                                    changed = true;
                                }

                                else if (std::stoi(bin->right.value) == -1 && !leftConst)
                                {
                                    result = bin->left;
                                    result.neg = !result.neg;
                                    replaceInst = true;
                                    changed = true;
                                }
                            }

                            else if (leftConst && std::stoi(bin->left.value) == 0)
                            {
                                result = TACValue{"0"};
                                replaceInst = true;
                                changed = true;
                            }

                            else if (!leftConst && !rightConst)
                            {
                                if (bin->left.value == bin->right.value && bin->left.neg == bin->right.neg)
                                {
                                    result = TACValue{"1"};
                                    replaceInst = true;
                                    changed = true;
                                }
                            }

                            else if (bin->left.neg && bin->right.neg)
                            {
                                bin->left.neg = false;
                                bin->right.neg = false;
                                changed = true;
                            }
                            break;

                        default:
                            break;
                    }

                    if (replaceInst)
                    {
                        auto assignInst = std::make_unique<TACAssign>();
                        assignInst->dest = bin->dest;
                        assignInst->source = result;
                        inst = std::move(assignInst);
                    }
                }
            }
        }
    }

    return changed;
}
