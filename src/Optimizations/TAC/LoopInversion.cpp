#include "Globals.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <variant>

void InvertLoops(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& loopInfo = TACFunc->getLoopInfo();

        for (TACLoop& loop : loopInfo.Loops)
        {
            TACBlock* guardBlock = IREditor<TACTypes>::insertBlockBefore(loop.Header, Message{Phase::LOOP_INV, IRTransformType::ADDED, "created initial guard block"});

            std::vector<TACBlock*> outsideParents;

            for (TACBlock* parent : loop.Header->Parents)
            {
                if (!loop.Blocks.contains(parent))
                    outsideParents.push_back(parent);
            }

            for (TACBlock* parent : outsideParents)
            {
                auto& insts = parent->Instructions;

                if (!insts.empty())
                {
                    if (insts.back()->type == TACInstType::JUMP)
                    {
                        TACJump* jump = static_cast<TACJump*>(insts.back().get());

                        if (jump->TargetBlock == loop.Header)
                            jump->TargetBlock = guardBlock;
                    }

                    else if (insts.back()->type == TACInstType::BRANCH)
                    {
                        TACBranch* branch = static_cast<TACBranch*>(insts.back().get());

                        if (branch->TrueTarget == loop.Header)
                            branch->TrueTarget = guardBlock;
                        if (branch->FalseTarget == loop.Header)
                            branch->FalseTarget = guardBlock;
                    }
                }

                IREditor<TACTypes>::removeEdge(parent, loop.Header);
                IREditor<TACTypes>::addEdge(parent, guardBlock);
            }

            auto cloneHeaderTo = [&](TACBlock* targetBlock) {
                std::unordered_map<std::string, std::string> tempMap;

                for (auto& inst : loop.Header->Instructions)
                {
                    IREditor<TACTypes>::cloneInstructionTo(inst.get(), targetBlock);

                    TACInstruction* cloned = targetBlock->Instructions.back().get();

                    TACInstOperands operands = GetTACInstOperands(cloned);

                    for (TACValue* use : operands.Uses)
                    {
                        if (std::holds_alternative<TACVariable>(*use))
                        {
                            TACVariable& var = std::get<TACVariable>(*use);

                            if (tempMap.count(var.OriginalName))
                                var.OriginalName = tempMap[var.OriginalName];
                        }
                    }

                    if (operands.Def != nullptr)
                    {
                        if (std::holds_alternative<TACVariable>(*operands.Def))
                        {
                            TACVariable& var = std::get<TACVariable>(*operands.Def);

                            if (var.OriginalName.size() >= 2 && var.OriginalName[0] == 't' && var.OriginalName[1] == '.')
                            {
                                tempMap[var.OriginalName] = "t." + std::to_string(TACFunc->NextTemp++);
                                var.OriginalName = tempMap[var.OriginalName];
                            }
                        }
                    }
                }
            };

            cloneHeaderTo(guardBlock);

            TACBranch* guardCond = static_cast<TACBranch*>(guardBlock->Instructions.back().get());

            IREditor<TACTypes>::addEdge(guardBlock, guardCond->TrueTarget);
            IREditor<TACTypes>::addEdge(guardBlock, guardCond->FalseTarget);

            if (!loop.End->Instructions.empty() && loop.End->Instructions.back()->type == TACInstType::JUMP)
            {
                TACJump* endJump = static_cast<TACJump*>(loop.End->Instructions.back().get());

                IREditor<TACTypes>::removeEdge(loop.End, endJump->TargetBlock);
                IREditor<TACTypes>::deleteInstruction(loop.End, endJump);
            }

            cloneHeaderTo(loop.End);

            TACBranch* endCond = static_cast<TACBranch*>(loop.End->Instructions.back().get());

            IREditor<TACTypes>::addEdge(loop.End, endCond->TrueTarget);
            IREditor<TACTypes>::addEdge(loop.End, endCond->FalseTarget);

            std::vector<TACBlock*> headerParents = loop.Header->Parents;
            std::vector<TACBlock*> headerChildren = loop.Header->Children;

            for (TACBlock* parent : headerParents)
                IREditor<TACTypes>::removeEdge(parent, loop.Header);

            for (TACBlock* child : headerChildren)
                IREditor<TACTypes>::removeEdge(loop.Header, child);

            IREditor<TACTypes>::deleteBlock(loop.Header, Message{Phase::LOOP_INV, IRTransformType::DELETED, "merged loop header into loop's end block"});
        }
    }
}
