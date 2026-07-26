#include "TACGenerator.h"
#include "TACEditor.h"
#include <algorithm>
#include <format>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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

            for (TACBlock* parent : header->Parents)
            {
                if (!loop.Blocks.contains(parent))
                {
                    outsideParents.push_back(parent);
                }
            }

            if (outsideParents.empty())
                continue;

            TACBlock* preheader = TACEditor::insertBlockBefore(header, Message{TACPass::LICM, TACTransformType::ADDED, std::format("created preheader 'Block - {}' for loop header 'Block - {}'", TACFunc->Blocks.size() + 1, header->ID)});

            for (TACBlock* parent : outsideParents)
            {
                TACEditor::removeEdge(parent, header);

                TACEditor::addEdge(parent, preheader);

                if (parent->Instructions.back()->type == TACType::JUMP)
                {
                    auto* jump = static_cast<TACJump*>(parent->Instructions.back().get());

                    if (jump->TargetBlock == header)
                        jump->TargetBlock = preheader;
                }

                else if (parent->Instructions.back()->type == TACType::BRANCH)
                {
                    auto* branch = static_cast<TACBranch*>(parent->Instructions.back().get());

                    if (branch->TrueTarget == header)
                        branch->TrueTarget = preheader;

                    if (branch->FalseTarget == header)
                        branch->FalseTarget = preheader;
                }
            }

            TACEditor::addEdge(preheader, header);

            TACEditor::appendInstruction(preheader, std::make_unique<TACJump>(header));

            preheaderMap[header] = preheader;
        }
    }

    TACEditor::reconstructSSA(TAC);

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
                TACVariable& var = std::get<TACVariable>(val);

                if (val.index() == 0 || invariants.contains(var))
                {
                    return true;
                }

                if (DefBlocksInfo.DefBlocks.find(var) != DefBlocksInfo.DefBlocks.end())
                {
                    if (!Loop.Blocks.contains(*DefBlocksInfo.DefBlocks.find(var)->second.begin()))
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

                        if (inst->type == TACType::ASSIGN)
                        {
                            TACAssign* assign = static_cast<TACAssign*>(inst.get());

                            if (isInvariant(assign->source))
                            {
                                destVar = std::get<TACVariable>(assign->dest);
                                canHoist = true;
                            }
                        }

                        if (inst->type == TACType::NEG)
                        {
                            TACNeg* neg = static_cast<TACNeg*>(inst.get());

                            if (isInvariant(neg->source))
                            {
                                destVar = std::get<TACVariable>(neg->dest);
                                canHoist = true;
                            }
                        }

                        else if (inst->type == TACType::BINARYOP)
                        {
                            TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                            if (binary->op != BinaryOp::DIV)
                            {
                                if (isInvariant(binary->left) && isInvariant(binary->right))
                                {
                                    destVar = std::get<TACVariable>(binary->dest);
                                    canHoist = true;
                                }
                            }
                        }

                        else if (inst->type == TACType::CALL)
                        {
                            TACCall* call = static_cast<TACCall*>(inst.get());

                            if (call->dest.has_value())
                            {
                                bool allArgsInvariant = std::all_of(call->args.begin(), call->args.end(), [&](const auto& arg) { return isInvariant(arg); });

                                if (allArgsInvariant)
                                {
                                    destVar = std::get<TACVariable>(call->dest.value());
                                    canHoist = true;
                                }
                            }
                        }

                        if (canHoist)
                        {
                            invariants.insert(destVar);

                            TACEditor::moveInstructionBefore(block, inst.get(), preheader, preheader->Instructions.back().get(), Message{TACPass::LICM, TACTransformType::MOVED, std::format("moved loop invariant instruction from 'Block - {}' to preheader", block->ID)});

                            changed = true;
                        }
                    }
                }
            }
        }
    }

    TACEditor::reconstructSSA(TAC);
}
