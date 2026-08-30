#include "IREditor.h"
#include "TACGenerator.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

bool EliminateDeadInstructions(TACFunction* TACFunc, TACVarUsesInfo& VarUsesInfo)
{
    bool changed = false;

    for (auto& Block : TACFunc->Blocks)
    {
        size_t initialSize = Block->Instructions.size();

        int returnIndex = -1;

        for (int i = Block->Instructions.size() - 1; i >= 0; --i)
        {
            auto& inst = Block->Instructions[i];

            TACValue destVar;

            if (inst->type == TACInstType::PHI)
            {
                TACPhi* phi = static_cast<TACPhi*>(inst.get());

                destVar = phi->variable;
            }

            else if (inst->type == TACInstType::ASSIGN)
            {
                TACAssign* assign = static_cast<TACAssign*>(inst.get());

                destVar = assign->dest;
            }

            else if (inst->type == TACInstType::NEG)
            {
                TACNeg* neg = static_cast<TACNeg*>(inst.get());

                destVar = neg->dest;
            }

            else if (inst->type == TACInstType::BINARYOP)
            {
                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                destVar = binary->dest;
            }

            else if (inst->type == TACInstType::CALL)
            {
                TACCall* call = static_cast<TACCall*>(inst.get());

                if (!call->dest.has_value())
                {
                    IREditor<TACTypes>::deleteInstruction(Block.get(), inst.get(), Message{Phase::DCE, IRTransformType::DELETED, "calls with no destination are dead because all functions are pure in the C subset supported by csalt"});
                    continue;
                }

                else
                    destVar = call->dest.value();
            }

            else if (inst->type == TACInstType::RETURN)
            {
                returnIndex = i;
                continue;
            }

            else
            {
                continue;
            }

            if (!VarUsesInfo.VarUses.contains(destVar) || VarUsesInfo.VarUses[destVar] == 0)
                IREditor<TACTypes>::deleteInstruction(Block.get(), inst.get(), Message{Phase::DCE, IRTransformType::DELETED, "definition was unused"});
        }

        if (returnIndex != -1)
        {
            for (int i = Block->Instructions.size() - 1; i > returnIndex; --i)
            {
                IREditor<TACTypes>::deleteInstruction(Block.get(), Block->Instructions[i].get(), Message{Phase::DCE, IRTransformType::DELETED, "instruction was after a return statement"});
            }
        }

        if (Block->Instructions.size() != initialSize)
        {
            changed = true;
        }
    }

    return changed;
}

bool EliminateUnreachableBlocks(TACFunction* TACFunc)
{
    if (TACFunc->Blocks.empty())
        return false;

    size_t entryBlockID = TACFunc->Blocks.front()->ID;
    size_t initialBlocks = TACFunc->Blocks.size();

    for (int i = TACFunc->Blocks.size() - 1; i >= 0; --i)
    {
        auto& block = TACFunc->Blocks[i];

        if (block->ID == entryBlockID)
            continue;


        if (block->Parents.empty())
        {
            std::vector<TACBlock*> blockParents = block->Parents;
            std::vector<TACBlock*> blockChildren = block->Children;

            for (TACBlock* parent : blockParents)
            {
                IREditor<TACTypes>::removeEdge(parent, block.get());
                IREditor<TACTypes>::removePhiSource(block.get(), parent);
            }

            for (TACBlock* child : blockChildren)
            {
                IREditor<TACTypes>::removeEdge(block.get(), child);
                IREditor<TACTypes>::removePhiSource(child, block.get());
            }

            IREditor<TACTypes>::deleteBlock(block.get(), Message{Phase::DCE, IRTransformType::DELETED, "block was unreachable"});
        }
    }

    return TACFunc->Blocks.size() != initialBlocks;
}

bool EliminateDeadFunctions(TAC& TAC)
{
    std::unordered_map<std::string, size_t> callCounts;

    size_t initialFuncCount = TAC.size();

    for (const auto& TACFunc : TAC)
    {
        for (const auto& Block : TACFunc->Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                if (inst->type == TACInstType::CALL)
                {
                    TACCall* call = static_cast<TACCall*>(inst.get());

                    callCounts[call->functionName]++;
                }
            }
        }
    }

    for (int i = TAC.size() - 1; i >= 0; --i)
    {
        auto& TACFunc = TAC[i];

        if (TACFunc->Name == "main")
            continue;

        if (callCounts[TACFunc->Name] == 0)
            IREditor<TACTypes>::deleteFunction(TAC, TACFunc.get(), Message{Phase::DCE, IRTransformType::DELETED, "function was never called"});
    };

    return initialFuncCount != TAC.size();
}

bool RemoveDeadCode(TAC& TAC)
{
    bool globalChanged = false;

    for (auto& TACFunc : TAC)
    {
        bool changed = true;

        while (changed)
        {
            changed = false;

            if (EliminateDeadInstructions(TACFunc.get(), TACFunc->getVarUsesInfo()))
            {
                changed = true;
                globalChanged = true;
            }

            if (EliminateUnreachableBlocks(TACFunc.get()))
            {
                changed = true;
                globalChanged = true;
            }
        }
    }

    return globalChanged || EliminateDeadFunctions(TAC);
}
