#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <format>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

std::unordered_map<TACBlock*, TACBlock*> createPreHeaders(TAC& TAC)
{
    std::unordered_map<TACBlock*, TACBlock*> preheaderMap;

    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TACFunc->getLoopInfo();

        for (TACLoop& loop : LoopInfo.Loops)
        {
            TACBlock* header = loop.Header;

            std::vector<TACBlock*> outsideParents;

            TACBlock* preheader = IREditor<TACTypes>::insertBlockBefore(header, Message{Phase::LICM, IRTransformType::ADDED, std::format("created preheader 'Block - {}' for loop header 'Block - {}'", std::ranges::max_element(TACFunc->Blocks, {}, &TACBlock::ID)->get()->ID + 1, header->ID)});

            for (TACBlock* parent : header->Parents)
            {
                if (loop.Blocks.contains(parent))
                    continue;

                if (parent->Instructions.back()->type == TACInstType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(parent->Instructions.back().get());

                    if (jump->TargetBlock == header)
                        jump->TargetBlock = preheader;
                }

                else if (parent->Instructions.back()->type == TACInstType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(parent->Instructions.back().get());

                    if (branch->TrueTarget == header)
                        branch->TrueTarget = preheader;

                    if (branch->FalseTarget == header)
                        branch->FalseTarget = preheader;
                }

                IREditor<TACTypes>::removeEdge(parent, header);
                IREditor<TACTypes>::addEdge(parent, preheader);
                IREditor<TACTypes>::remapPhiSources(header, parent, {preheader});
            }

            IREditor<TACTypes>::addEdge(preheader, header);

            IREditor<TACTypes>::appendInstruction(preheader, std::make_unique<TACJump>(header));

            preheaderMap[header] = preheader;
        }
    }

    return preheaderMap;
}

void HoistLoopInvariants(TAC& TAC)
{
    std::unordered_map<TACBlock*, TACBlock*> preheaderMap = createPreHeaders(TAC);

    for (auto& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TACFunc->getLoopInfo();
        TACDefBlocksInfo& DefBlocksInfo = TACFunc->getDefBlocksInfo();
        TACDominatorInfo& DomInfo = TACFunc->getDominatorInfo();

        for (TACLoop& Loop : LoopInfo.Loops)
        {
            TACBlock* preheader = preheaderMap[Loop.Header];

            std::unordered_set<TACVariable> invariants;

            auto isInvariant = [&](TACValue val) -> bool {
                if (std::holds_alternative<int>(val) || invariants.contains(std::get<TACVariable>(val)))
                {
                    return true;
                }

                if (DefBlocksInfo.DefBlocks.find(val) != DefBlocksInfo.DefBlocks.end())
                {
                    if (!Loop.Blocks.contains(*DefBlocksInfo.DefBlocks.find(val)->second.begin()))
                    {
                        return true;
                    }
                }

                return false;
            };

            bool changed = true;

            while (changed)
            {
                changed = false;

                for (TACBlock* block : Loop.Blocks)
                {
                    if (!DomInfo.Dominators[Loop.End].contains(block))
                        continue;

                    for (int i = block->Instructions.size() - 1; i >= 0; --i)
                    {
                        auto& inst = block->Instructions[i];

                        TACVariable destVar;
                        bool canHoist = false;

                        TACInstOperands operands = GetTACInstOperands(inst.get());

                        bool allOperandsInvariant = std::all_of(operands.Uses.begin(), operands.Uses.end(), [&isInvariant](auto& op) { return isInvariant(*op); });

                        if (allOperandsInvariant)
                        {
                            if (inst->type == TACInstType::BINARYOP)
                            {
                                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                                if (binary->op == BinaryOp::DIV)
                                    continue;
                            }

                            if (operands.Def == nullptr)
                            {
                                // all function calls are guaranteed to be pure due to the restricted scope of the compiler
                                if (inst->type == TACInstType::CALL)
                                    canHoist = true;
                                else
                                    continue;
                            }

                            if (!canHoist)
                            {
                                invariants.insert(std::get<TACVariable>(*operands.Def));
                                canHoist = true;
                            }
                        }

                        if (canHoist)
                        {
                            IREditor<TACTypes>::moveInstructionBefore(block, inst.get(), preheader, preheader->Instructions.back().get(), Message{Phase::LICM, IRTransformType::MOVED, std::format("moved loop invariant instruction from 'Block - {}' to preheader", block->ID)});

                            changed = true;
                        }
                    }
                }
            }
        }
    }
}
