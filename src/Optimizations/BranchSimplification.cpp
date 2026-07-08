#include <vector>
#include <memory>
#include "TACGenerator.h"
#include "ConstantFolding.h"

void SimplifyBranches(std::vector<std::unique_ptr<TACFunction>>& TAC)
{
    for (auto& Function : TAC)
    {
        for (auto& block : Function->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (auto br = dynamic_cast<TACBranch*>(inst.get()))
                {
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
                            jumpInst = std::make_unique<TACJump>(br->TrueTarget);
                        else
                            jumpInst = std::make_unique<TACJump>(br->FalseTarget);

                        inst = std::move(jumpInst);
                    }
                }
            }
        }
    }
}
