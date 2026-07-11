#include <memory>
#include "TACGenerator.h"
#include "ConstantFolding.h"

bool SimplifyBranches(TAC& TAC)
{
    bool changed = false;

    for (TACFunction& Function : TAC)
    {
        for (auto& block : Function.Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BRANCH)
                {
                    TACBranch* br = static_cast<TACBranch*>(inst.get());

                    bool leftConst = isConstant(br->cond.Left.value);
                    bool rightConst = isConstant(br->cond.Right.value);

                    if (leftConst && rightConst)
                    {
                        int left = std::stoi(br->cond.Left.value);
                        int right = std::stoi(br->cond.Right.value);

                        bool result;

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

                        std::unique_ptr<TACJump> jumpInst;

                        if (result)
                        {
                            jumpInst = std::make_unique<TACJump>(br->TrueTarget);

                            auto& jumpBlock = *(std::find_if(Function.Blocks.begin(), Function.Blocks.end(), [br](const auto& b) { return b->ID == br->FalseTarget; }));

                            std::erase(jumpBlock->Parents, block.get());
                        }
                        else
                        {
                            jumpInst = std::make_unique<TACJump>(br->FalseTarget);

                            auto& jumpBlock = *(std::find_if(Function.Blocks.begin(), Function.Blocks.end(), [br](const auto& b) { return b->ID == br->TrueTarget; }));

                            std::erase(jumpBlock->Parents, block.get());
                        }

                        inst = std::move(jumpInst);

                        changed = true;
                    }

                    else if (br->TrueTarget == br->FalseTarget)
                    {
                        inst = std::make_unique<TACJump>(br->TrueTarget);

                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}
