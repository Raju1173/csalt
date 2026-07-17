#include "CFGBuilder.h"
#include "TACGenerator.h"
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

std::unordered_map<TACBlock*, TACBlock*> createPreHeaders(TAC& TAC)
{
    std::unordered_map<TACBlock*, TACBlock*> preheaderMap;

    for (TACFunction& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TAC.getLoopInfo(TACFunc);

        for (TACLoop& loop : LoopInfo.Loops)
        {
            auto preheader = std::make_unique<TACBlock>(TACFunc.Blocks.size() + 1);

            for (TACBlock* parent : loop.Header->Parents)
            {
                if (!loop.Blocks.contains(parent))
                {
                    preheader->Parents.push_back(parent);

                    for (TACBlock*& child : parent->Children)
                    {
                        if (child == loop.Header)
                        {
                            child = preheader.get();
                        }
                    }

                    if (!parent->Instructions.empty())
                    {
                        if (parent->Instructions.back()->type == TACType::JUMP)
                        {
                            static_cast<TACJump*>(parent->Instructions.back().get())->TargetBlock = preheader->ID;
                        }

                        else
                        {
                            TACBranch* br = static_cast<TACBranch*>(parent->Instructions.back().get());

                            if (br->TrueTarget == loop.Header->ID)
                            {
                                br->TrueTarget = preheader->ID;
                            }

                            if (br->FalseTarget == loop.Header->ID)
                            {
                                br->FalseTarget = preheader->ID;
                            }
                        }
                    }
                }
            }

            std::erase_if(loop.Header->Parents, [&loop](TACBlock* parent) {
                return !loop.Blocks.contains(parent);
            });

            preheader->Children.push_back(loop.Header);
            loop.Header->Parents.push_back(preheader.get());

            preheader->Instructions.push_back(std::make_unique<TACJump>(loop.Header->ID));

            preheaderMap[loop.Header] = preheader.get();

            TACFunc.Blocks.insert(std::ranges::find_if(TACFunc.Blocks, [&loop](size_t ID) { return ID == loop.Header->ID; }, &TACBlock::ID), std::move(preheader));

            LoopInfo.isValid = false;
            TAC.getDominatorInfo(TACFunc).isValid = false;
            TAC.getDominatorTreeInfo(TACFunc).isValid = false;
        }
    }

    return preheaderMap;
}

void HoistLoopInvariants(TAC& TAC)
{
    std::unordered_map<TACBlock*, TACBlock*> preheaderMap = createPreHeaders(TAC);

    for (TACFunction& TACFunc : TAC)
    {
        TACLoopInfo& LoopInfo = TAC.getLoopInfo(TACFunc);
        TACDefBlocksInfo& DefBlocksInfo = TAC.getDefBlocksInfo(TACFunc);
        TACDominatorInfo& DomInfo = TAC.getDominatorInfo(TACFunc);

        for (TACLoop& Loop : LoopInfo.Loops)
        {
            std::unordered_set<std::string> invariants;

            bool changed = true;

            while (changed)
            {
                changed = false;

                for (TACBlock* block : Loop.Blocks)
                {
                    if (DomInfo.Dominators[Loop.End].contains(block))
                    {
                        for (auto& inst : block->Instructions)
                        {
                            switch (inst->type)
                            {
                                case TACType::ASSIGN:
                                    {
                                        TACAssign* assign = static_cast<TACAssign*>(inst.get());

                                        if (isConstant(assign->source.value) || !Loop.Blocks.contains(DefBlocksInfo.DefBlocks[assign->source.value]) || invariants.contains(assign->source.value))
                                        {
                                            invariants.insert(assign->dest.value);

                                            TACBlock* preheader = preheaderMap[Loop.Header];

                                            preheader->Instructions.insert(preheader->Instructions.end() - 1, std::move(inst));

                                            for (auto& headerInst : Loop.Header->Instructions)
                                            {
                                                if (headerInst->type == TACType::PHI)
                                                {
                                                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                                                    for (PhiArgument& arg : phi->args)
                                                    {
                                                        if (arg.SourceID ==)
                                                    }
                                                }
                                            }

                                            changed = true;
                                        }
                                    }
                                    break;

                                case TACType::BINARYOP:
                                    {
                                        TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                                        if (binary->op != BinaryOp::DIV)
                                        {
                                            bool leftInvariant = false;
                                            bool rightInvariant = false;

                                            if (isConstant(binary->left.value) || !Loop.Blocks.contains(DefBlocksInfo.DefBlocks[binary->left.value]) || invariants.contains(binary->left.value))
                                            {
                                                leftInvariant = true;
                                            }

                                            if (isConstant(binary->right.value) || !Loop.Blocks.contains(DefBlocksInfo.DefBlocks[binary->right.value]) || invariants.contains(binary->right.value))
                                            {
                                                rightInvariant = true;
                                            }

                                            if (leftInvariant && rightInvariant)
                                            {
                                                invariants.insert(binary->dest.value);

                                                TACBlock* preheader = preheaderMap[Loop.Header];

                                                preheader->Instructions.insert(preheader->Instructions.end() - 1, std::move(inst));

                                                changed = true;
                                            }
                                        }
                                    }
                                    break;

                                case TACType::CALL:
                                    {
                                        TACCall* call = static_cast<TACCall*>(inst.get());

                                        if (call->dest.has_value())
                                        {
                                            bool argsInvariant = true;

                                            for (TACValue& arg : call->args)
                                            {
                                                if (!(isConstant(arg.value) || !Loop.Blocks.contains(DefBlocksInfo.DefBlocks[arg.value]) || invariants.contains(arg.value)))
                                                {
                                                    argsInvariant = false;
                                                }
                                            }

                                            if (argsInvariant)
                                            {
                                                invariants.insert(call->dest.value().value);

                                                TACBlock* preheader = preheaderMap[Loop.Header];

                                                preheader->Instructions.insert(preheader->Instructions.end() - 1, std::move(inst));

                                                changed = true;
                                            }
                                        }
                                    }
                                    break;
                            }
                        }
                    }

                    std::erase_if(block->Instructions, [](auto& inst) { return inst.get() == nullptr; });
                }
            }
        }

        DefBlocksInfo.isValid = false;
        TAC.getVarUsesInfo(TACFunc).isValid = false;
    }
}
