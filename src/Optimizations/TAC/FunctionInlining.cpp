#include "FunctionInlining.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACAnalyses.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <algorithm>
#include <cassert>
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
        TACLivenessInfo& LivenessInfo = TACFunc->getLivenessInfo();

        std::vector<TACInstruction*> calls;

        for (auto& Block : TACFunc->Blocks)
        {
            for (auto& inst : Block->Instructions)
            {
                if (inst->type == TACInstType::CALL)
                {
                    if (static_cast<TACCall*>(inst.get())->functionName != TACFunc->Name)
                        calls.push_back(inst.get());
                }
            }
        }

        std::unordered_map<std::string, int> inlineCount;

        for (TACInstruction* callInst : calls)
        {
            TACCall* call = static_cast<TACCall*>(callInst);

            TACBlock* splitStartBlock = nullptr;
            size_t callIndex = 0;

            for (auto& block : TACFunc->Blocks)
            {
                auto foundBlock = std::find_if(block->Instructions.begin(), block->Instructions.end(), [&callInst](auto& inst) { return inst.get() == callInst; });

                if (foundBlock != block->Instructions.end())
                {
                    splitStartBlock = block.get();

                    callIndex = static_cast<size_t>(foundBlock - block->Instructions.begin());
                    break;
                }
            }

            int pressureAtCallBlock = LivenessInfo.BlockLiveness[splitStartBlock].LiveIn.size();

            int peakPressureInCalledFunction = 0;

            for (auto& [block, blockLiveness] : LivenessInfo.BlockLiveness)
            {
                peakPressureInCalledFunction = std::max(peakPressureInCalledFunction, blockLiveness.MaxPressure);
            }

            int allocatableRegs = gCompilerOptions[Phase::FPO].enabled ? 15 : 14;

            // this heuristic is 'overly pessimistic' because a missed optimization is better than a regression for this compiler...
            if (pressureAtCallBlock + peakPressureInCalledFunction > allocatableRegs - 2)
                continue;

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

                auto movedCall = std::find_if(calls.begin(), calls.end(), [&inst](TACInstruction* i) { return i == inst.get(); });

                IREditor<TACTypes>::moveInstructionTo(splitStartBlock, inst.get(), splitEndBlock, Message{});

                if (movedCall != calls.end())
                    *movedCall = splitEndBlock->Instructions.back().get();
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

                        var.OriginalName = var.OriginalName + "_" + calledFunction->Name + std::to_string(inlineCount[calledFunction->Name]);
                        var.SSAName = var.OriginalName;
                    }
                }
            }

            TACBlock* clonedEntry = clonedBlockMap[calledFunction->Blocks[0].get()];

            for (int i = static_cast<int>(calledFunction->Parameters.size()) - 1; i >= 0; i--)
            {
                TACVariable paramVar = TACVariable{calledFunction->Parameters[i] + "_" + calledFunction->Name + std::to_string(inlineCount[calledFunction->Name])};

                IREditor<TACTypes>::addInstructionBefore(clonedEntry, clonedEntry->Instructions.front().get(), std::make_unique<TACAssign>(paramVar, call->args[i]), Message{});
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

                    IREditor<TACTypes>::addEdge(newBlock, jump->TargetBlock);
                }

                else if (lastInst->type == TACInstType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(lastInst);

                    branch->TrueTarget = clonedBlockMap[branch->TrueTarget];
                    branch->FalseTarget = clonedBlockMap[branch->FalseTarget];

                    IREditor<TACTypes>::addEdge(newBlock, branch->TrueTarget);
                    IREditor<TACTypes>::addEdge(newBlock, branch->FalseTarget);
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
