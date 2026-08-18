#include "SSAConstructor.h"
#include "IRDebugger.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
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

    auto renameUse = [](TACValue& val) {
        if (std::holds_alternative<TACVariable>(val))
        {
            TACVariable& var = std::get<TACVariable>(val);

            if (var.OriginalName.size() >= 2 && var.OriginalName[1] == '.')
                return;

            std::stack<int>& VersionStack = VarStacks[var.OriginalName].first;

            if (!VersionStack.empty())
            {
                var.SSAName = var.OriginalName + std::to_string(VersionStack.top());
            }
        }
    };

    auto renamePhiArgs = [&Block](TACBlock* targetBlock) {
        for (auto& targetInst : targetBlock->Instructions)
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
    };

    for (auto& inst : Block->Instructions)
    {
        auto renameDef = [&pushed](TACVariable& var) {
            if (var.OriginalName.size() >= 2 && var.OriginalName[1] == '.')
                return;

            auto& [versionStack, versionCounter] = VarStacks[var.OriginalName];

            versionStack.push(versionCounter);
            versionCounter++;

            var.SSAName = var.OriginalName + std::to_string(versionStack.top());
            pushed.push_back(var.OriginalName);
        };

        switch (inst->type)
        {
            case TACInstType::PHI:
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    renameDef(std::get<TACVariable>(phi->variable));
                    break;
                }

            case TACInstType::ASSIGN:
                {
                    TACAssign* assign = static_cast<TACAssign*>(inst.get());

                    renameUse(assign->source);
                    renameDef(std::get<TACVariable>(assign->dest));
                    break;
                }

            case TACInstType::NEG:
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    renameUse(binary->left);
                    renameUse(binary->right);
                    renameDef(std::get<TACVariable>(binary->dest));
                    break;
                }

            case TACInstType::BINARYOP:
                {
                    TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                    renameUse(binary->left);
                    renameUse(binary->right);
                    renameDef(std::get<TACVariable>(binary->dest));
                    break;
                }

            case TACInstType::CALL:
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    for (TACValue& arg : call->args)
                    {
                        renameUse(arg);
                    }

                    if (call->dest.has_value())
                    {
                        renameDef(std::get<TACVariable>(call->dest.value()));
                    }
                    break;
                }

            case TACInstType::BRANCH:
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    renameUse(branch->cond.Left);
                    renameUse(branch->cond.Right);
                    break;
                }

            case TACInstType::SELECT:
                {
                    TACSelect* select = static_cast<TACSelect*>(inst.get());

                    renameUse(select->cond.Left);
                    renameUse(select->cond.Right);
                    renameUse(select->TrueVal);
                    renameUse(select->FalseVal);
                    renameDef(std::get<TACVariable>(select->dest));
                    break;
                }

            case TACInstType::RETURN:
                {
                    TACReturn* ret = static_cast<TACReturn*>(inst.get());

                    if (ret->ReturnValue.has_value())
                    {
                        renameUse(ret->ReturnValue.value());
                    }
                    break;
                }
        }
    }

    for (TACBlock* child : Block->Children)
    {
        renamePhiArgs(child);
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
    Debugger::Notify(Phase::TAC);
}
