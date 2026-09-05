#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <memory>

// TODO : have to add if conversion heuristics...

void IfConversion(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        std::unordered_set<TACBlock*> blocksToDelete;

        for (auto& Block : TACFunc->Blocks)
        {
            if (!blocksToDelete.contains(Block.get()))
                continue;

            if (!Block->Instructions.empty() && Block->Instructions.back()->type == TACInstType::BRANCH)
            {
                TACBranch* branch = static_cast<TACBranch*>(Block->Instructions.back().get());

                bool triangle = false;

                TACBlock* triangleConditionalBlock = nullptr;
                TACBlock* triangleMergeBlock = nullptr;

                if (branch->TrueTarget->Parents.size() == 1 && branch->TrueTarget->Children.size() == 1 && branch->TrueTarget->Children[0] == branch->FalseTarget)
                {
                    triangle = true;
                    triangleConditionalBlock = branch->TrueTarget;
                    triangleMergeBlock = branch->FalseTarget;
                }

                else if (branch->FalseTarget->Parents.size() == 1 && branch->FalseTarget->Children.size() == 1 && branch->FalseTarget->Children[0] == branch->TrueTarget)
                {
                    triangle = true;
                    triangleConditionalBlock = branch->FalseTarget;
                    triangleMergeBlock = branch->TrueTarget;
                }

                if (triangle)
                {
                    for (auto& inst : triangleMergeBlock->Instructions)
                    {
                        if (inst->type == TACInstType::PHI)
                        {
                            TACPhi* phi = static_cast<TACPhi*>(inst.get());

                            TACValue trueVal;
                            TACValue falseVal;

                            for (PhiArgument& arg : phi->args)
                            {
                                if (triangleConditionalBlock == branch->TrueTarget)
                                {
                                    if (arg.SourceBlock == branch->TrueTarget)
                                        trueVal = arg.Value;

                                    else if (arg.SourceBlock == Block.get())
                                        falseVal = arg.Value;
                                }

                                else
                                {
                                    if (arg.SourceBlock == Block.get())
                                        trueVal = arg.Value;

                                    else if (arg.SourceBlock == branch->FalseTarget)
                                        falseVal = arg.Value;
                                }
                            }

                            IREditor<TACTypes>::replaceInstruction(triangleMergeBlock, phi, std::make_unique<TACSelect>(phi->variable, branch->cond, trueVal, falseVal), Message{});
                        }
                    }

                    if (triangleMergeBlock->Instructions.front()->type != TACInstType::SELECT)
                        continue;

                    while (triangleConditionalBlock->Instructions[0]->type != TACInstType::BRANCH && triangleConditionalBlock->Instructions[0]->type != TACInstType::JUMP)
                    {
                        IREditor<TACTypes>::moveInstructionBefore(triangleConditionalBlock, triangleConditionalBlock->Instructions[0].get(), Block.get(), branch, Message{});
                    }

                    IREditor<TACTypes>::removeEdge(Block.get(), triangleConditionalBlock);

                    IREditor<TACTypes>::replaceInstruction(Block.get(), branch, std::make_unique<TACJump>(triangleMergeBlock), Message{});
                    IREditor<TACTypes>::addEdge(Block.get(), triangleMergeBlock);

                    blocksToDelete.insert(triangleConditionalBlock);

                    continue;
                }

                bool diamond = (branch->TrueTarget->Parents.size() == 1 && branch->FalseTarget->Parents.size() == 1) && (branch->TrueTarget->Children.size() == 1 && branch->FalseTarget->Children.size() == 1) && (branch->TrueTarget->Children[0] == branch->FalseTarget->Children[0]);

                if (diamond)
                {
                    TACBlock* mergeBlock = branch->TrueTarget->Children[0];

                    for (auto& inst : mergeBlock->Instructions)
                    {
                        if (inst->type == TACInstType::PHI)
                        {
                            TACPhi* phi = static_cast<TACPhi*>(inst.get());

                            TACValue trueVal;
                            TACValue falseVal;

                            for (PhiArgument& arg : phi->args)
                            {
                                if (arg.SourceBlock == branch->TrueTarget)
                                    trueVal = arg.Value;

                                else if (arg.SourceBlock == branch->FalseTarget)
                                    falseVal = arg.Value;
                            }

                            IREditor<TACTypes>::replaceInstruction(mergeBlock, phi, std::make_unique<TACSelect>(phi->variable, branch->cond, trueVal, falseVal), Message{});
                        }
                    }

                    if (mergeBlock->Instructions.front()->type != TACInstType::SELECT)
                        continue;

                    while (branch->TrueTarget->Instructions[0]->type != TACInstType::BRANCH && branch->TrueTarget->Instructions[0]->type != TACInstType::JUMP)
                    {
                        IREditor<TACTypes>::moveInstructionBefore(branch->TrueTarget, branch->TrueTarget->Instructions[0].get(), Block.get(), branch, Message{});
                    }

                    while (branch->FalseTarget->Instructions[0]->type != TACInstType::BRANCH && branch->FalseTarget->Instructions[0]->type != TACInstType::JUMP)
                    {
                        IREditor<TACTypes>::moveInstructionBefore(branch->FalseTarget, branch->FalseTarget->Instructions[0].get(), Block.get(), branch, Message{});
                    }

                    IREditor<TACTypes>::removeEdge(Block.get(), branch->TrueTarget);
                    IREditor<TACTypes>::removeEdge(Block.get(), branch->FalseTarget);

                    IREditor<TACTypes>::replaceInstruction(Block.get(), branch, std::make_unique<TACJump>(mergeBlock), Message{});
                    IREditor<TACTypes>::addEdge(Block.get(), mergeBlock);

                    blocksToDelete.insert(branch->TrueTarget);
                    blocksToDelete.insert(branch->FalseTarget);
                }
            }
        }

        for (TACBlock* deadBlock : blocksToDelete)
        {
            IREditor<TACTypes>::deleteBlock(deadBlock, Message{});
        }
    }
}
