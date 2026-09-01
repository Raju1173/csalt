#include "FunctionInlining.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <unordered_map>

void InlineFunctions(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        std::vector<std::pair<size_t, TACBlock*>> calls;

        for (auto& Block : TACFunc->Blocks)
        {
            for (size_t i = 0; i < Block->Instructions.size(); i++)
            {
                if (Block->Instructions[i]->type == TACInstType::CALL)
                {
                    calls.push_back({i, Block.get()});
                }
            }
        }

        for (auto& callLocation : calls)
        {
            TACBlock* splitStartBlock = callLocation.second;

            TACCall* call = static_cast<TACCall*>(splitStartBlock->Instructions[callLocation.first].get());

            TACBlock* splitEndBlock = IREditor<TACTypes>::insertBlockAfter(splitStartBlock, Message{});

            for (TACBlock* child : splitStartBlock->Children)
            {
                IREditor<TACTypes>::removeEdge(splitStartBlock, child);
            }

            for (size_t i = callLocation.first + 1; i < splitStartBlock->Instructions.size(); i++)
            {
                IREditor<TACTypes>::moveInstructionTo(splitStartBlock, splitStartBlock->Instructions[callLocation.first + 1].get(), splitEndBlock, Message{});
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

            std::unordered_map<TACBlock*, TACBlock*> clonedBlockMap;

            for (auto& block : calledFunction->Blocks)
            {
                clonedBlockMap[block.get()] = IREditor<TACTypes>::cloneBlockBefore(block.get(), splitEndBlock, Message{});
            }
        }
    }
}
