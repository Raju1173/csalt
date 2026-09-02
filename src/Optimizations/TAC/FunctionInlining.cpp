#include "FunctionInlining.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

void InlineFunctions(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        std::vector<std::pair<TACInstruction*, TACBlock*>> calls;

        for (auto& Block : TACFunc->Blocks)
        {
            for (size_t i = 0; i < Block->Instructions.size(); i++)
            {
                if (Block->Instructions[i]->type == TACInstType::CALL)
                {
                    calls.push_back({Block->Instructions[i].get(), Block.get()});
                }
            }
        }

        std::unordered_map<std::string, int> inlineCount;

        for (auto& [callInst, splitStartBlock] : calls)
        {
            TACCall* call = static_cast<TACCall*>(callInst);

            size_t callIndex = static_cast<size_t>(std::find_if(splitStartBlock->Instructions.begin(), splitStartBlock->Instructions.end(), [&callInst](auto& inst) { return callInst == inst.get(); }) - splitStartBlock->Instructions.begin());

            TACBlock* splitEndBlock = IREditor<TACTypes>::insertBlockAfter(splitStartBlock, Message{});

            inlineCount[call->functionName]++;

            std::vector<TACBlock*> splitStartChildren = splitStartBlock->Children;

            for (TACBlock* child : splitStartChildren)
            {
                IREditor<TACTypes>::removeEdge(splitStartBlock, child);
            }

            while (callIndex + 1 < splitStartBlock->Instructions.size())
            {
                auto& inst = splitStartBlock->Instructions[callIndex + 1];

                if (inst->type == TACInstType::CALL)
                {
                    auto callLocation = std::find_if(calls.begin(), calls.end(), [&inst](auto& callLocation) { return callLocation.first == inst.get(); });

                    if (callLocation != calls.end())
                        callLocation->second = splitEndBlock;
                }

                IREditor<TACTypes>::moveInstructionTo(splitStartBlock, inst.get(), splitEndBlock, Message{});
            }

            if (splitEndBlock->Instructions.back()->type == TACInstType::JUMP)
            {
                TACJump* jump = static_cast<TACJump*>(splitEndBlock->Instructions.back().get());

                IREditor<TACTypes>::addEdge(splitEndBlock, jump->TargetBlock);
            }

            else if (splitEndBlock->Instructions.back()->type == TACInstType::BRANCH)
            {
                TACBranch* branch = static_cast<TACBranch*>(splitEndBlock->Instructions.back().get());

                IREditor<TACTypes>::addEdge(splitEndBlock, branch->TrueTarget);
                IREditor<TACTypes>::addEdge(splitEndBlock, branch->FalseTarget);
            }

            TACFunction* calledFunction = std::find_if(TAC.begin(), TAC.end(), [&call](auto& func) { return func->Name == call->functionName; })->get();

            std::map<TACBlock*, TACBlock*> clonedBlockMap;

            for (auto& block : calledFunction->Blocks)
            {
                clonedBlockMap[block.get()] = IREditor<TACTypes>::cloneBlockBefore(block.get(), splitEndBlock, Message{});
            }

            std::unordered_map<TACVariable, TACValue> paramToArgMap;

            for (size_t i = 0; i < calledFunction->Parameters.size(); i++)
            {
                paramToArgMap[TACVariable{calledFunction->Parameters[i]}] = call->args[i];
            }

            for (auto& [oldBlock, newBlock] : clonedBlockMap)
            {
                for (auto& inst : newBlock->Instructions)
                {
                    for (TACValue* operand : GetTACInstOperands(inst.get()).Operands)
                    {
                        if (!std::get_if<TACVariable>(operand))
                            continue;

                        TACVariable& var = std::get<TACVariable>(*operand);

                        if (paramToArgMap.contains(var))
                        {
                            *operand = paramToArgMap[var];
                            continue;
                        }

                        var.OriginalName = var.OriginalName + "_" + calledFunction->Name + std::to_string(inlineCount[calledFunction->Name]);
                        var.SSAName = var.OriginalName;
                    }
                }
            }

            IREditor<TACTypes>::appendInstruction(splitStartBlock, std::make_unique<TACJump>(clonedBlockMap[calledFunction->Blocks[0].get()]), Message{});

            IREditor<TACTypes>::addEdge(splitStartBlock, clonedBlockMap[calledFunction->Blocks[0].get()]);

            for (auto& [oldBlock, newBlock] : clonedBlockMap)
            {
                TACInstruction* lastInst = newBlock->Instructions.back().get();

                if (lastInst->type == TACInstType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(lastInst);

                    jump->TargetBlock = clonedBlockMap[jump->TargetBlock];
                }

                else if (lastInst->type == TACInstType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(lastInst);

                    branch->TrueTarget = clonedBlockMap[branch->TrueTarget];
                    branch->FalseTarget = clonedBlockMap[branch->FalseTarget];
                }

                else if (lastInst->type == TACInstType::RETURN)
                {
                    TACReturn* ret = static_cast<TACReturn*>(lastInst);

                    if (ret->ReturnValue.has_value())
                        if (call->dest.has_value())
                            IREditor<TACTypes>::addInstructionBefore(newBlock, ret, std::make_unique<TACAssign>(call->dest.value(), ret->ReturnValue.value()), Message{});

                    IREditor<TACTypes>::replaceInstruction(newBlock, ret, std::make_unique<TACJump>(splitEndBlock), Message{});
                    IREditor<TACTypes>::addEdge(newBlock, splitEndBlock);
                }
            }

            IREditor<TACTypes>::deleteInstruction(splitStartBlock, call, Message{});
        }
    }
}
