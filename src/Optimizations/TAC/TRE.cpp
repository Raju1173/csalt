#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <memory>
#include <string>
#include <unordered_map>

struct TailCallLocation
{
    TACBlock* block;
    size_t callIdx;
};

void EliminateTailRecursions(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        std::vector<TailCallLocation> sites;

        for (auto& block : TACFunc->Blocks)
        {
            if (block->Instructions.size() < 2)
                continue;

            for (size_t i = 0; i < block->Instructions.size() - 1; i++)
            {
                if (block->Instructions[i]->type == TACInstType::CALL && block->Instructions[i + 1]->type == TACInstType::RETURN)
                {
                    TACCall* call = static_cast<TACCall*>(block->Instructions[i].get());
                    TACReturn* ret = static_cast<TACReturn*>(block->Instructions[i + 1].get());

                    if (call->functionName == TACFunc->Name && call->dest == ret->ReturnValue)
                    {
                        sites.push_back({block.get(), i});
                    }
                }
            }
        }

        if (sites.empty())
            continue;

        TACBlock* entryBlock = TACFunc->Blocks.front().get();
        TACBlock* headerBlock = IREditor<TACTypes>::insertBlockAfter(entryBlock, Message{Phase::TRE, IRTransformType::ADDED, "added header block to turn function body into a 'while(true)' loop"});

        std::vector<TACBlock*> entryBlockChildren = entryBlock->Children;

        for (TACBlock* child : entryBlockChildren)
        {
            IREditor<TACTypes>::addEdge(headerBlock, child);
            IREditor<TACTypes>::removeEdge(entryBlock, child);
        }

        while (!entryBlock->Instructions.empty())
        {
            IREditor<TACTypes>::moveInstructionTo(entryBlock, entryBlock->Instructions[0].get(), headerBlock, Message{Phase::TRE, IRTransformType::MOVED, "TODO : add message info"});
        }

        for (std::string param : TACFunc->Parameters)
        {
            IREditor<TACTypes>::appendInstruction(entryBlock, std::make_unique<TACAssign>(TACVariable{param}, TACVariable{param}));
        }

        IREditor<TACTypes>::appendInstruction(entryBlock, std::make_unique<TACJump>(headerBlock));
        IREditor<TACTypes>::addEdge(entryBlock, headerBlock);

        for (TailCallLocation& site : sites)
        {
            TACBlock* Block = site.block;

            TACCall* call = static_cast<TACCall*>(Block->Instructions[site.callIdx].get());
            TACReturn* ret = static_cast<TACReturn*>(Block->Instructions[site.callIdx + 1].get());

            std::unordered_map<std::string, TACVariable> tempArgs;

            for (size_t j = 0; j < call->args.size(); j++)
            {
                std::string param = TACFunc->Parameters[j];
                tempArgs[param] = TACVariable{"t." + std::to_string(TACFunc->NextTemp++)};
                IREditor<TACTypes>::addInstructionBefore(Block, call, std::make_unique<TACAssign>(tempArgs[param], call->args[j]), Message{Phase::TRE, IRTransformType::ADDED, "TODO : add message info"});
            }

            for (std::string param : TACFunc->Parameters)
            {
                IREditor<TACTypes>::addInstructionBefore(Block, call, std::make_unique<TACAssign>(TACVariable{param}, tempArgs[param]), Message{Phase::TRE, IRTransformType::ADDED, "TODO : add message info"});
            }

            IREditor<TACTypes>::replaceInstruction(Block, call, std::make_unique<TACJump>(headerBlock), Message{Phase::TRE, IRTransformType::REPLACED, "converted recursive tail call into a jump to the function's body loop header"});
            IREditor<TACTypes>::addEdge(Block, headerBlock);

            IREditor<TACTypes>::deleteInstruction(Block, ret, Message{Phase::TRE, IRTransformType::DELETED, "TODO : add message info"});
        }
    }
}
