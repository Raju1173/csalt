#include <format>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include "TACGenerator.h"
#include "TACEditor.h"

bool SimplifyBranches(TAC& TAC)
{
    bool changed = false;

    for (auto& Function : TAC)
    {
        for (auto& block : Function->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BRANCH)
                {
                    TACBranch* br = static_cast<TACBranch*>(inst.get());

                    if (br->TrueTarget == br->FalseTarget)
                    {
                        Message message = Message{TACPass::BRANCH_SIMPLIFICATION, TACTransformType::REPLACED, std::format("simplified 'cond ? B{} : B{}' to 'jump B{}'", br->TrueTarget->ID, br->FalseTarget->ID, br->TrueTarget->ID)};

                        TACEditor::replaceInstruction(block.get(), inst.get(), std::make_unique<TACJump>(br->TrueTarget), message);

                        changed = true;
                        break;
                    }

                    auto* left = std::get_if<int>(&br->cond.Left);
                    auto* right = std::get_if<int>(&br->cond.Right);

                    bool result;

                    if ((left && right) || (br->cond.Left == br->cond.Right))
                    {
                        if (left && right)
                        {
                            switch (br->cond.Op)
                            {
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
                        }

                        else if (br->cond.Left == br->cond.Right)
                        {
                            switch (br->cond.Op)
                            {
                                case BinaryOp::DOUBLE_EQUAL:
                                    result = true;
                                    break;

                                case BinaryOp::NOT_EQUAL:
                                    result = false;
                                    break;

                                case BinaryOp::GREATER:
                                    result = false;
                                    break;

                                case BinaryOp::GREATER_EQUAL:
                                    result = true;
                                    break;

                                case BinaryOp::LESS:
                                    result = false;
                                    break;

                                case BinaryOp::LESS_EQUAL:
                                    result = true;
                                    break;
                            }
                        }

                        std::unique_ptr<TACJump> jumpInst;

                        if (result)
                        {
                            jumpInst = std::make_unique<TACJump>(br->TrueTarget);

                            TACEditor::removeEdge(block.get(), br->FalseTarget);
                        }

                        else
                        {
                            jumpInst = std::make_unique<TACJump>(br->FalseTarget);

                            TACEditor::removeEdge(block.get(), br->TrueTarget);
                        }

                        std::string info = std::format("constant branch '{} {} {} ? B{} : B{}' simplified to 'jump B{}'", TACValToStr(br->cond.Left), BinaryOpToStr[std::to_underlying(br->cond.Op)], TACValToStr(br->cond.Right), std::to_string(br->TrueTarget->ID), std::to_string(br->FalseTarget->ID), std::to_string(jumpInst->TargetBlock->ID));

                        TACEditor::replaceInstruction(block.get(), inst.get(), std::move(jumpInst), Message{TACPass::BRANCH_SIMPLIFICATION, TACTransformType::REPLACED, info});

                        changed = true;
                    }
                }
            }
        }
    }

    if (changed)
        TACEditor::reconstructSSA(TAC);

    return changed;
}
