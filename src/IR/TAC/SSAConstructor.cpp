#include "SSAConstructor.h"
#include "IRDebugger.h"
#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <print>
#include <stack>
#include <string>
#include <utility>
#include <variant>
#include <vector>

void InsertPhiNodes(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (const auto& [var, defBlocks] : TACFunc->getDefBlocksInfo().DefBlocks)
        {
            std::unordered_set<TACBlock*> worklist = defBlocks;

            std::unordered_set<TACBlock*> added;

            while (!worklist.empty())
            {
                TACBlock* block = *worklist.begin();

                worklist.erase(worklist.begin());

                for (TACBlock* frontier : TACFunc->getFrontierInfo().Frontiers[block])
                {
                    if (added.find(frontier) == added.end())
                    {
                        frontier->Instructions.insert(frontier->Instructions.begin(), std::make_unique<TACPhi>(var));

                        added.insert(frontier);

                        worklist.insert(frontier);
                    }
                }
            }
        }
    }

    Debugger::Notify(Phase::TAC_PHI_INS);
}

void RemoveDeadPhiNodes(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        TACVarUsesInfo& VarUsesInfo = TACFunc->getVarUsesInfo();

        for (auto& block : TACFunc->Blocks)
        {
            std::erase_if(block->Instructions, [&VarUsesInfo](auto& inst) {
                if (inst->type == TACInstType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    return (!VarUsesInfo.VarUses.contains(phi->variable) || VarUsesInfo.VarUses[phi->variable] == 0) || phi->args.size() < 2;
                };

                return false;
            });
        }
    }
}

std::unordered_map<std::string, std::pair<std::stack<int>, int>> VarStacks;

void RenameBlock(TACBlock* Block, TACDominatorTreeInfo& DomTreeInfo)
{
    std::vector<std::string> pushed;

    for (auto& inst : Block->Instructions)
    {
        for (TACValue* val : GetTACInstOperands(inst.get()).Uses)
        {
            if (inst->type != TACInstType::PHI)
            {
                if (std::holds_alternative<TACVariable>(*val))
                {
                    TACVariable& var = std::get<TACVariable>(*val);

                    if (var.OriginalName.size() >= 2 && var.OriginalName[1] == '.')
                        continue;

                    std::stack<int>& VersionStack = VarStacks[var.OriginalName].first;

                    if (!VersionStack.empty())
                    {
                        var.SSAName = var.OriginalName + std::to_string(VersionStack.top());
                    }
                }
            }
        }

        if (GetTACInstOperands(inst.get()).Def != nullptr)
        {
            TACVariable& var = std::get<TACVariable>(*GetTACInstOperands(inst.get()).Def);

            if (var.OriginalName.size() >= 2 && var.OriginalName[1] == '.')
                continue;

            auto& [versionStack, versionCounter] = VarStacks[var.OriginalName];

            versionStack.push(versionCounter);
            versionCounter++;

            var.SSAName = var.OriginalName + std::to_string(versionStack.top());
            pushed.push_back(var.OriginalName);
        }
    }

    for (TACBlock* child : Block->Children)
    {
        for (auto& targetInst : child->Instructions)
        {
            if (targetInst->type == TACInstType::PHI)
            {
                TACPhi* phi = static_cast<TACPhi*>(targetInst.get());

                TACVariable& var = std::get<TACVariable>(phi->variable);

                if (!VarStacks[var.OriginalName].first.empty())
                {
                    phi->args.push_back(PhiArgument{Block, TACVariable{var.OriginalName, var.OriginalName + std::to_string(VarStacks[var.OriginalName].first.top())}});
                }
            }
        }
    }

    for (size_t i = 0; i < DomTreeInfo.DominatorTree[Block].size(); i++)
    {
        RenameBlock(DomTreeInfo.DominatorTree[Block][i], DomTreeInfo);
    }

    for (std::string var : pushed)
    {
        VarStacks[var].first.pop();
    }
}

void RenameVariables(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        TACFunc->getDefBlocksInfo().isValid = false;

        RenameBlock(TACFunc->Blocks[0].get(), TACFunc->getDominatorTreeInfo());

        VarStacks.clear();
    }

    RemoveDeadPhiNodes(TAC);

    Debugger::Notify(Phase::TAC_RENAME);
}

void SplitCriticalEdges(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        std::vector<std::pair<TACBlock*, TACBlock*>> criticalEdges;

        for (auto& block : TACFunc->Blocks)
        {
            bool hasPhi = false;

            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACInstType::PHI)
                {
                    hasPhi = true;
                    break;
                }
            }

            if (hasPhi && block->Parents.size() > 1)
            {
                for (TACBlock* parent : block->Parents)
                {
                    if (parent->Children.size() > 1)
                        criticalEdges.push_back(std::make_pair(parent, block.get()));
                }
            }
        }

        for (auto [src, dst] : criticalEdges)
        {
            TACBlock* splitBlock = IREditor<TACTypes>::insertBlockBefore(dst);

            IREditor<TACTypes>::appendInstruction(splitBlock, std::make_unique<TACJump>(dst));

            IREditor<TACTypes>::removeEdge(src, dst);
            IREditor<TACTypes>::addEdge(src, splitBlock);
            IREditor<TACTypes>::addEdge(splitBlock, dst);

            if (!src->Instructions.empty())
            {
                if (src->Instructions.back().get()->type == TACInstType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(src->Instructions.back().get());

                    if (branch->TrueTarget == dst)
                        branch->TrueTarget = splitBlock;

                    if (branch->FalseTarget == dst)
                        branch->FalseTarget = splitBlock;
                }

                else if (src->Instructions.back().get()->type == TACInstType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(src->Instructions.back().get());

                    jump->TargetBlock = splitBlock;
                }
            }

            for (auto& inst : dst->Instructions)
            {
                if (inst->type == TACInstType::PHI)
                {
                    for (PhiArgument& arg : static_cast<TACPhi*>(inst.get())->args)
                    {
                        if (arg.SourceBlock == src)
                            arg.SourceBlock = splitBlock;
                    }
                }
            }
        }
    }

    Debugger::Notify(Phase::TAC_EDGE_SPLIT);
}

void ResolvePhiNodes(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (auto& block : TACFunc->Blocks)
        {
            for (size_t i = 0; i < block->Instructions.size(); i++)
            {
                auto& inst = block->Instructions[i];

                if (inst->type == TACInstType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    for (PhiArgument& arg : phi->args)
                    {
                        auto assign = std::make_unique<TACAssign>();

                        assign->dest = phi->variable;
                        assign->source = arg.Value;

                        if (arg.SourceBlock->ID == block->ID)
                        {
                            block->Instructions.insert(block->Instructions.begin() + i, std::move(assign));
                            i++;
                        }

                        else if (!arg.SourceBlock->Instructions.empty())
                            arg.SourceBlock->Instructions.insert(arg.SourceBlock->Instructions.end() - 1, std::move(assign));

                        else
                            arg.SourceBlock->Instructions.push_back(std::move(assign));
                    }
                }
            }

            std::erase_if(block->Instructions, [](const auto& inst) { return inst->type == TACInstType::PHI; });
        }
    }

    Debugger::Notify(Phase::TAC_PHI_RES);
}
