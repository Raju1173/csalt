#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "TACEditor.h"
#include <cstddef>
#include <memory>
#include <print>
#include <stack>
#include <string>
#include <utility>
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
}

std::unordered_map<std::string, std::pair<std::stack<int>, int>> VarStacks;

void RenameBlock(TACBlock* Block, TACDominatorTreeInfo& DomTreeInfo)
{
    std::vector<std::string> pushed;

    auto renameUse = [](TACValue& val) {
        if (val.index() == 1)
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
            if (targetInst->type == TACType::PHI)
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
        auto renameDef = [&pushed, &inst](TACVariable& var) {
            if (var.OriginalName.size() >= 2 && var.OriginalName[1] == '.')
                return;

            auto& [versionStack, versionCounter] = VarStacks[var.OriginalName];

            versionStack.push(versionCounter);
            versionCounter++;

            if (var.SSAName != var.OriginalName && var.SSAName != var.OriginalName + std::to_string(versionStack.top()) && inst->type != TACType::PHI)
            {
                inst->History.push_back(Message{TACPass::SSA_RECONSTRUCTION, TACTransformType::RENAMED, std::format("renamed definition {} to {}", var.SSAName, var.OriginalName + std::to_string(versionStack.top()))});
            }

            var.SSAName = var.OriginalName + std::to_string(versionStack.top());
            pushed.push_back(var.OriginalName);
        };

        switch (inst->type)
        {
            case TACType::PHI:
                {
                    auto* phi = static_cast<TACPhi*>(inst.get());

                    renameDef(std::get<TACVariable>(phi->variable));
                    break;
                }

            case TACType::ASSIGN:
                {
                    auto* assign = static_cast<TACAssign*>(inst.get());

                    renameUse(assign->source);
                    renameDef(std::get<TACVariable>(assign->dest));
                    break;
                }

            case TACType::NEG:
                {
                    auto* neg = static_cast<TACNeg*>(inst.get());

                    renameUse(neg->source);
                    renameDef(std::get<TACVariable>(neg->dest));
                    break;
                }

            case TACType::BINARYOP:
                {
                    auto* binary = static_cast<TACBinaryOp*>(inst.get());

                    renameUse(binary->left);
                    renameUse(binary->right);
                    renameDef(std::get<TACVariable>(binary->dest));
                    break;
                }

            case TACType::CALL:
                {
                    auto* call = static_cast<TACCall*>(inst.get());

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

            case TACType::BRANCH:
                {
                    auto* branch = static_cast<TACBranch*>(inst.get());

                    renameUse(branch->cond.Left);
                    renameUse(branch->cond.Right);
                    break;
                }

            case TACType::SELECT:
                {
                    auto* select = static_cast<TACSelect*>(inst.get());

                    renameUse(select->cond.Left);
                    renameUse(select->cond.Right);
                    renameUse(select->TrueVal);
                    renameUse(select->FalseVal);
                    renameDef(std::get<TACVariable>(select->dest));
                    break;
                }

            case TACType::RETURN:
                {
                    auto* ret = static_cast<TACReturn*>(inst.get());

                    if (ret->ReturnValue.has_value())
                    {
                        renameUse(ret->ReturnValue.value());
                    }
                    break;
                }
        }
    }

    for (auto& child : Block->Children)
    {
        renamePhiArgs(child);
    }

    for (size_t i = 0; i < DomTreeInfo.DominatorTree[Block].size(); i++)
    {
        RenameBlock(DomTreeInfo.DominatorTree[Block][i], DomTreeInfo);
    }

    for (const std::string& var : pushed)
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

        for (auto& Block : TACFunc->Blocks)
        {
            std::erase_if(Block->Instructions, [](auto& inst) {
                if (inst->type == TACType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    return phi->args.size() < 2;
                }

                return false;
            });
        }
    }
}

void ResolvePhiNodes(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (auto& block : TACFunc->Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    for (PhiArgument& arg : phi->args)
                    {
                        for (auto& sourceBlock : TACFunc->Blocks)
                        {
                            if (sourceBlock->ID == arg.SourceBlock->ID)
                            {
                                auto assign = std::make_unique<TACAssign>();

                                assign->dest = phi->variable;

                                assign->source = arg.Value;

                                if (sourceBlock->Instructions.size() >= 1)
                                    sourceBlock->Instructions.insert(sourceBlock->Instructions.end() - 1, std::move(assign));

                                else
                                    sourceBlock->Instructions.push_back(std::move(assign));

                                break;
                            }
                        }
                    }
                }
            }

            std::erase_if(block->Instructions, [](const auto& inst) { return inst->type == TACType::PHI; });
        }
    }
}
