#include "Globals.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <memory>

bool conditionIsLoopVariant(TACBranch* branch, TACLoop* enclosingLoop, TACDefBlocksInfo& DefBlocksInfo)
{
    if (!enclosingLoop)
        return true;

    for (TACValue* operand : GetTACInstOperands(branch).Uses)
    {
        if (std::holds_alternative<int>(*operand))
            continue;

        // using "DefBlocksSet.begin()" since this is a post SSA optimization thus, there can only be one def block...
        if (!DefBlocksInfo.DefBlocks[*operand].empty() && enclosingLoop->Blocks.contains(*DefBlocksInfo.DefBlocks[*operand].begin()))
            return true;
    }

    return false;
}

void IfConversion(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TACFunc->getLoopInfo();
        TACLivenessInfo& LivenessInfo = TACFunc->getLivenessInfo();
        TACDefBlocksInfo& DefBlocksInfo = TACFunc->getDefBlocksInfo();

        std::unordered_set<TACBlock*> blocksToDelete;

        for (auto& Block : TACFunc->Blocks)
        {
            if (blocksToDelete.contains(Block.get()))
                continue;

            if (Block->Instructions.empty() || Block->Instructions.back()->type != TACInstType::BRANCH)
                continue;

            TACBranch* branch = static_cast<TACBranch*>(Block->Instructions.back().get());

            TACBlock* mergeBlock = nullptr;
            TACBlock* trueValBlock = nullptr;
            TACBlock* falseValBlock = nullptr;

            std::vector<TACBlock*> condBlocks;

            bool isTrueTriangle = branch->TrueTarget->Parents.size() == 1 && branch->TrueTarget->Children.size() == 1 && branch->TrueTarget->Children[0] == branch->FalseTarget && branch->FalseTarget->Parents.size() == 2;

            bool isFalseTriangle = branch->FalseTarget->Parents.size() == 1 && branch->FalseTarget->Children.size() == 1 && branch->FalseTarget->Children[0] == branch->TrueTarget && branch->TrueTarget->Parents.size() == 2;

            bool isDiamond = branch->TrueTarget->Parents.size() == 1 && branch->FalseTarget->Parents.size() == 1 && branch->TrueTarget->Children.size() == 1 && branch->FalseTarget->Children.size() == 1 && branch->TrueTarget->Children[0] == branch->FalseTarget->Children[0] && branch->TrueTarget->Children[0]->Parents.size() == 2;

            if (isTrueTriangle)
            {
                mergeBlock = branch->FalseTarget;
                trueValBlock = branch->TrueTarget;
                falseValBlock = Block.get();
                condBlocks = {branch->TrueTarget};
            }

            else if (isFalseTriangle)
            {
                mergeBlock = branch->TrueTarget;
                trueValBlock = Block.get();
                falseValBlock = branch->FalseTarget;
                condBlocks = {branch->FalseTarget};
            }

            else if (isDiamond)
            {
                mergeBlock = branch->TrueTarget->Children[0];
                trueValBlock = branch->TrueTarget;
                falseValBlock = branch->FalseTarget;
                condBlocks = {branch->TrueTarget, branch->FalseTarget};
            }

            else
            {
                continue;
            }

            int mergePhiCount = 0;

            for (auto& inst : mergeBlock->Instructions)
            {
                if (inst->type == TACInstType::PHI)
                    mergePhiCount++;
                else
                    break;
            }

            int pressureAtMerge = LivenessInfo.BlockLiveness[mergeBlock].LiveIn.size() + mergePhiCount;

            int allocatableRegs = gCompilerOptions[Phase::FPO].enabled ? 15 : 14;

            if (pressureAtMerge > allocatableRegs)
                continue;

            TACLoop* enclosingLoop = nullptr;

            for (TACLoop* loop : LoopInfo.allLoops)
            {
                if (loop->Blocks.contains(Block.get()) && (!enclosingLoop || loop->Blocks.size() < enclosingLoop->Blocks.size()))
                    enclosingLoop = loop;
            }

            if (!conditionIsLoopVariant(branch, enclosingLoop, DefBlocksInfo))
                continue;

            for (auto& inst : mergeBlock->Instructions)
            {
                if (inst->type != TACInstType::PHI)
                    continue;

                TACPhi* phi = static_cast<TACPhi*>(inst.get());

                TACValue trueVal;
                TACValue falseVal;

                for (PhiArgument& arg : phi->args)
                {
                    if (arg.SourceBlock == trueValBlock)
                        trueVal = arg.Value;

                    else if (arg.SourceBlock == falseValBlock)
                        falseVal = arg.Value;
                }

                IREditor<TACTypes>::replaceInstruction(mergeBlock, phi, std::make_unique<TACSelect>(phi->variable, branch->cond, trueVal, falseVal), Message{});
            }

            if (mergeBlock->Instructions.front()->type != TACInstType::SELECT)
                continue;

            for (TACBlock* condBlock : condBlocks)
            {
                while (condBlock->Instructions[0]->type != TACInstType::BRANCH && condBlock->Instructions[0]->type != TACInstType::JUMP)
                {
                    IREditor<TACTypes>::moveInstructionBefore(condBlock, condBlock->Instructions[0].get(), Block.get(), branch, Message{});
                }
            }

            for (TACBlock* condBlock : condBlocks)
            {
                IREditor<TACTypes>::removeEdge(Block.get(), condBlock);
                blocksToDelete.insert(condBlock);
            }

            IREditor<TACTypes>::replaceInstruction(Block.get(), branch, std::make_unique<TACJump>(mergeBlock), Message{});
            IREditor<TACTypes>::addEdge(Block.get(), mergeBlock);
        }

        for (TACBlock* deadBlock : blocksToDelete)
        {
            IREditor<TACTypes>::deleteBlock(deadBlock, Message{});
        }
    }
}
