#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <variant>

TACDominatorInfo& TACFunction::getDominatorInfo()
{
    TACDominatorInfo& DomInfo = Metadata.DomInfo;

    if (!DomInfo.isValid)
    {
        DomInfo.isValid = true;

        DomInfo.Dominators.clear();

        if (Blocks.empty())
            return DomInfo;

        TACBlock* entryBlock = Blocks[0].get();

        std::unordered_set<TACBlock*> universalSet;

        for (auto& block : Blocks)
        {
            universalSet.insert(block.get());
        }

        DomInfo.Dominators[entryBlock] = {entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            DomInfo.Dominators[Blocks[j].get()] = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                TACBlock* curBlock = Blocks[j].get();

                if (curBlock == entryBlock)
                    continue;

                std::unordered_set<TACBlock*> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = DomInfo.Dominators[curBlock->Parents[0]];

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::unordered_set<TACBlock*> currentIntersection;

                        for (TACBlock* dom : NewDominators)
                        {
                            if (DomInfo.Dominators[curBlock->Parents[k]].contains(dom))
                            {
                                currentIntersection.insert(dom);
                            }
                        }

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(curBlock);

                if (NewDominators != DomInfo.Dominators[curBlock])
                {
                    DomInfo.Dominators[curBlock] = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }

    return DomInfo;
}

TACDominatorTreeInfo& TACFunction::getDominatorTreeInfo()
{
    TACDominatorInfo& DomInfo = getDominatorInfo();
    TACDominatorTreeInfo& DomTreeInfo = Metadata.DomTreeInfo;

    TACBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    if (!DomTreeInfo.isValid)
    {
        DomTreeInfo.isValid = true;

        DomTreeInfo.DominatorTree.clear();

        for (auto& block : Blocks)
        {
            for (TACBlock* dom : DomInfo.Dominators[block.get()])
            {
                if (dom == block.get())
                    continue;

                size_t size = DomInfo.Dominators[dom].size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                DomTreeInfo.DominatorTree[nearestDominator].push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }

    return DomTreeInfo;
}

void ComputeBlockFrontiers(TACBlock* Block, TACDominatorInfo& DomInfo, TACDominatorTreeInfo& DomTreeInfo, TACFrontierInfo& FrontierInfo)
{
    if (!Block->Instructions.empty())
    {
        if (Block->Instructions.back()->type == TACInstType::JUMP)
        {
            TACJump* jump = static_cast<TACJump*>(Block->Instructions.back().get());

            if (!DomInfo.Dominators[jump->TargetBlock].contains(Block) || jump->TargetBlock == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(jump->TargetBlock);
            }
        }

        else if (Block->Instructions.back()->type == TACInstType::BRANCH)
        {
            TACBranch* br = static_cast<TACBranch*>(Block->Instructions.back().get());

            if (!DomInfo.Dominators[br->TrueTarget].contains(Block) || br->TrueTarget == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(br->TrueTarget);
            }

            if (!DomInfo.Dominators[br->FalseTarget].contains(Block) || br->FalseTarget == Block)
            {
                FrontierInfo.Frontiers[Block].push_back(br->FalseTarget);
            }
        }
    }

    for (TACBlock* domChild : DomTreeInfo.DominatorTree[Block])
    {
        ComputeBlockFrontiers(domChild, DomInfo, DomTreeInfo, FrontierInfo);

        for (TACBlock* childFrontier : FrontierInfo.Frontiers[domChild])
        {
            if ((!DomInfo.Dominators[childFrontier].contains(Block) || childFrontier == Block))
            {
                FrontierInfo.Frontiers[Block].push_back(childFrontier);
            }
        }
    }
}

TACFrontierInfo& TACFunction::getFrontierInfo()
{
    TACDominatorInfo& domInfo = getDominatorInfo();
    TACDominatorTreeInfo& domTreeInfo = getDominatorTreeInfo();
    TACFrontierInfo& frontierInfo = Metadata.FrontierInfo;

    if (!frontierInfo.isValid)
    {
        frontierInfo.isValid = true;

        frontierInfo.Frontiers.clear();

        ComputeBlockFrontiers(Blocks[0].get(), domInfo, domTreeInfo, frontierInfo);
    }

    return frontierInfo;
}

TACVarUsesInfo& TACFunction::getVarUsesInfo()
{
    TACVarUsesInfo& VarUsesInfo = Metadata.VarUsesInfo;

    if (!VarUsesInfo.isValid)
    {
        VarUsesInfo.isValid = true;

        VarUsesInfo.VarUses.clear();

        for (const auto& Block : Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                for (TACValue* val : GetTACInstOperands(inst.get()).Uses)
                {
                    VarUsesInfo.VarUses[*val]++;
                }
            }
        }
    }

    return VarUsesInfo;
}

TACDefBlocksInfo& TACFunction::getDefBlocksInfo()
{
    TACDefBlocksInfo& DefBlocksInfo = Metadata.DefBlocksInfo;

    if (!DefBlocksInfo.isValid)
    {
        DefBlocksInfo.isValid = true;

        DefBlocksInfo.DefBlocks.clear();

        for (const auto& Block : Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                if (GetTACInstOperands(inst.get()).Def != nullptr)
                {
                    DefBlocksInfo.DefBlocks[*GetTACInstOperands(inst.get()).Def].insert(Block.get());
                }
            }
        }
    }

    return DefBlocksInfo;
}

void findLoopBlocks(TACLoop& loop, TACBlock* block)
{
    if (block == loop.Header)
        return;

    for (TACBlock* parent : block->Parents)
    {
        if (loop.Blocks.contains(parent))
            continue;

        loop.Blocks.insert(parent);
        findLoopBlocks(loop, parent);
    }
}

TACLoopInfo& TACFunction::getLoopInfo()
{
    TACDominatorInfo& DominatorInfo = getDominatorInfo();
    TACLoopInfo& LoopInfo = Metadata.LoopInfo;

    if (!LoopInfo.isValid)
    {
        LoopInfo.isValid = true;

        LoopInfo.LoopForest.clear();
        LoopInfo.allLoops.clear();

        std::unordered_map<TACBlock*, std::vector<TACBlock*>> backedgesToHeader;

        for (auto& Block : Blocks)
        {
            for (TACBlock* child : Block->Children)
            {
                if (DominatorInfo.Dominators[Block.get()].contains(child))
                {
                    backedgesToHeader[child].push_back(Block.get());
                }
            }
        }

        std::vector<std::unique_ptr<TACLoop>> flatLoops;

        for (auto& [header, latches] : backedgesToHeader)
        {
            auto loop = std::make_unique<TACLoop>();

            loop->Header = header;
            loop->Latches = latches;
            loop->Blocks.insert(header);

            for (TACBlock* latch : latches)
            {
                loop->Blocks.insert(latch);
                findLoopBlocks(*loop, latch);
            }

            std::vector<TACBlock*> outsideParents;

            for (TACBlock* parent : header->Parents)
            {
                if (!loop->Blocks.contains(parent))
                    outsideParents.push_back(parent);
            }

            if (outsideParents.size() == 1 && outsideParents[0]->Children.size() == 1)
                loop->Preheader = outsideParents[0];

            flatLoops.push_back(std::move(loop));
        }

        std::sort(flatLoops.begin(), flatLoops.end(), [](auto& a, auto& b) { return a->Blocks.size() < b->Blocks.size(); });

        for (size_t i = 0; i < flatLoops.size(); i++)
        {
            for (size_t j = i + 1; j < flatLoops.size(); j++)
            {
                if (flatLoops[j].get()->Blocks.contains(flatLoops[i].get()->Header))
                {
                    flatLoops[i].get()->parentLoop = flatLoops[j].get();
                    break;
                }
            }
        }

        for (auto& loop : flatLoops)
        {
            LoopInfo.allLoops.push_back(loop.get());

            if (loop->parentLoop != nullptr)
                loop->parentLoop->childLoops.push_back(std::move(loop));

            else
                LoopInfo.LoopForest.push_back(std::move(loop));
        }
    }

    return LoopInfo;
}

TACInstruction* getDefiningInst(TACValue& val, TACDefBlocksInfo& DefBlocksInfo)
{
    if (DefBlocksInfo.DefBlocks[val].empty())
        return nullptr;

    TACBlock* defBlock = *DefBlocksInfo.DefBlocks[val].begin();

    for (auto& blockInst : defBlock->Instructions)
    {
        TACInstOperands operands = GetTACInstOperands(blockInst.get());

        if (operands.Def != nullptr && *operands.Def == val)
            return blockInst.get();
    }

    return nullptr;
}

TACInductionVariableInfo& TACFunction::getInductionVariableInfo()
{
    TACLoopInfo& LoopInfo = getLoopInfo();
    TACDefBlocksInfo& DefBlocksInfo = getDefBlocksInfo();
    TACInductionVariableInfo& IndVarInfo = Metadata.InductionVariableInfo;

    if (!IndVarInfo.isValid)
    {
        IndVarInfo.isValid = true;
        IndVarInfo.InductionVariables.clear();

        for (TACLoop* loop : LoopInfo.allLoops)
        {
            if (loop->Latches.size() != 1)
                continue;

            TACBlock* latch = loop->Latches[0];

            for (auto& inst : loop->Header->Instructions)
            {
                if (inst->type != TACInstType::PHI)
                    continue;

                TACPhi* phi = static_cast<TACPhi*>(inst.get());

                auto initValIt = std::find_if(phi->args.begin(), phi->args.end(), [loop](PhiArgument& arg) { return !loop->Blocks.contains(arg.SourceBlock); });

                auto latchValIt = std::find_if(phi->args.begin(), phi->args.end(), [latch](PhiArgument& arg) { return arg.SourceBlock == latch; });

                if (initValIt == phi->args.end() || latchValIt == phi->args.end())
                    continue;

                TACValue initVal = initValIt->Value;
                TACValue latchVal = latchValIt->Value;

                TACValue currVal = latchVal;
                TACBinaryOp* stepInst = nullptr;

                while (true)
                {
                    TACInstruction* defInst = getDefiningInst(currVal, DefBlocksInfo);

                    if (!defInst)
                        break;

                    if (defInst->type == TACInstType::BINARYOP)
                    {
                        stepInst = static_cast<TACBinaryOp*>(defInst);
                        break;
                    }

                    TACInstOperands operands = GetTACInstOperands(defInst);

                    if (operands.Uses.size() == 1)
                        currVal = **operands.Uses.begin();
                    else
                        break;
                }

                if (!stepInst)
                    continue;

                if (stepInst->op != BinaryOp::PLUS && stepInst->op != BinaryOp::MINUS)
                    continue;

                if (stepInst->left != phi->variable && stepInst->right != phi->variable)
                    continue;

                if (stepInst->op == BinaryOp::MINUS && stepInst->left != phi->variable)
                    continue;

                TACValue stepVal = (stepInst->left == phi->variable) ? stepInst->right : stepInst->left;

                bool isStepValInvariant = false;

                if (std::holds_alternative<int>(stepVal))
                {
                    isStepValInvariant = true;
                }

                else
                {
                    if (DefBlocksInfo.DefBlocks[stepVal].empty())
                    {
                        isStepValInvariant = true;
                    }

                    else
                    {
                        isStepValInvariant = true;

                        for (TACBlock* defBlock : DefBlocksInfo.DefBlocks[stepVal])
                        {
                            if (loop->Blocks.contains(defBlock))
                            {
                                isStepValInvariant = false;
                                break;
                            }
                        }
                    }
                }

                if (!isStepValInvariant)
                    continue;

                IndVarInfo.InductionVariables[loop].push_back(TACInductionVariable{phi, initVal, stepVal, stepInst});
            }
        }
    }

    return IndVarInfo;
}

TACLivenessInfo& TACFunction::getLivenessInfo()
{
    TACLivenessInfo& LivenessInfo = Metadata.LivenessInfo;

    if (!LivenessInfo.isValid)
    {
        LivenessInfo.isValid = true;

        LivenessInfo.BlockLiveness.clear();

        std::unordered_map<TACBlock*, std::unordered_set<TACValue>> UpwardUses;
        std::unordered_map<TACBlock*, std::unordered_set<TACValue>> Defs;
        std::unordered_map<TACBlock*, std::unordered_set<TACValue>> PhiUsesAtParent;

        for (auto& Block : Blocks)
        {
            std::unordered_set<TACValue>& upward = UpwardUses[Block.get()];
            std::unordered_set<TACValue>& defs = Defs[Block.get()];

            for (auto& inst : Block->Instructions)
            {
                if (inst->type == TACInstType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    defs.insert(phi->variable);

                    for (PhiArgument& arg : phi->args)
                    {
                        PhiUsesAtParent[arg.SourceBlock].insert(arg.Value);
                    }

                    continue;
                }

                TACInstOperands operands = GetTACInstOperands(inst.get());

                for (TACValue* use : operands.Uses)
                {
                    if (std::holds_alternative<TACVariable>(*use) && !defs.contains(*use))
                    {
                        upward.insert(*use);
                    }
                }

                if (operands.Def != nullptr && std::holds_alternative<TACVariable>(*operands.Def))
                {
                    defs.insert(*operands.Def);
                }
            }
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (auto& Block : Blocks)
            {
                std::unordered_set<TACValue> OUT = PhiUsesAtParent[Block.get()];

                for (TACBlock* child : Block.get()->Children)
                {
                    for (TACValue val : LivenessInfo.BlockLiveness[child].LiveIn)
                    {
                        OUT.insert(val);
                    }
                }

                std::unordered_set<TACValue> IN = OUT;

                for (TACValue def : Defs[Block.get()])
                {
                    IN.erase(def);
                }

                for (TACValue use : UpwardUses[Block.get()])
                {
                    IN.insert(use);
                }

                if (IN != LivenessInfo.BlockLiveness[Block.get()].LiveIn || OUT != LivenessInfo.BlockLiveness[Block.get()].LiveOut)
                {
                    LivenessInfo.BlockLiveness[Block.get()].LiveIn = std::move(IN);
                    LivenessInfo.BlockLiveness[Block.get()].LiveOut = std::move(OUT);
                    changed = true;
                }
            }
        }

        for (auto& Block : Blocks)
        {
            std::unordered_set<TACValue> live = LivenessInfo.BlockLiveness[Block.get()].LiveOut;

            int maxPressure = live.size();

            for (int i = static_cast<int>(Block.get()->Instructions.size()) - 1; i >= 0; i--)
            {
                auto& inst = Block.get()->Instructions[i];

                if (inst->type == TACInstType::PHI)
                    continue;

                TACInstOperands operands = GetTACInstOperands(inst.get());

                if (operands.Def != nullptr && std::holds_alternative<TACVariable>(*operands.Def))
                {
                    live.erase(*operands.Def);
                }

                maxPressure = std::max(maxPressure, (int)live.size());

                for (TACValue* use : operands.Uses)
                {
                    if (std::holds_alternative<TACVariable>(*use))
                    {
                        live.insert(*use);
                    }
                }

                maxPressure = std::max(maxPressure, (int)live.size());
            }

            for (auto& inst : Block.get()->Instructions)
            {
                if (inst->type != TACInstType::PHI)
                    break;

                live.erase(static_cast<TACPhi*>(inst.get())->variable);
            }

            maxPressure = std::max(maxPressure, static_cast<int>(live.size()));

            LivenessInfo.BlockLiveness[Block.get()].MaxPressure = maxPressure;
        }
    }

    return LivenessInfo;
}
