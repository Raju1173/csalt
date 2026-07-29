#include "TACEditor.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include <algorithm>
#include <vector>

void TACEditor::reconstructSSA(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (auto& block : TACFunc->Blocks)
        {
            while (!block->Instructions.empty() && block->Instructions.front()->type == TACType::PHI)
            {
                if (block->Instructions[0]->History.empty() && block->Instructions[0]->deadSiblings.preceding.empty() && block->Instructions[0]->deadSiblings.trailing.empty())
                {
                    block->Instructions.erase(block->Instructions.begin());
                }

                else
                {
                    cascadeDeletion<TACInstruction>(block->Instructions, 0, &block->LastDeadInstruction);
                }
            }
        }

        invalidateAllAnalyses(TACFunc->Metadata);
    }

    InsertPhiNodes(TAC);

    RenameVariables(TAC);
}

void TACEditor::appendInstruction(TACBlock* block, std::unique_ptr<TACInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    block->Instructions.push_back(std::move(inst));

    invalidateDataflowAnalyses(block->Function->Metadata);
}

void TACEditor::addInstructionBefore(TACBlock* block, TACInstruction* target, std::unique_ptr<TACInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    inst->deadSiblings.preceding.insert(inst->deadSiblings.preceding.begin(), std::make_move_iterator(target->deadSiblings.preceding.begin()), std::make_move_iterator(target->deadSiblings.preceding.end()));

    target->deadSiblings.preceding.clear();

    block->Instructions.insert(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; }), std::move(inst));

    invalidateDataflowAnalyses(block->Function->Metadata);
}

void TACEditor::addInstructionAfter(TACBlock* block, TACInstruction* target, std::unique_ptr<TACInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    inst->deadSiblings.trailing.insert(inst->deadSiblings.trailing.end(), std::make_move_iterator(target->deadSiblings.trailing.begin()), std::make_move_iterator(target->deadSiblings.trailing.end()));

    target->deadSiblings.trailing.clear();

    block->Instructions.insert(std::next(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; })), std::move(inst));

    invalidateDataflowAnalyses(block->Function->Metadata);
}

void TACEditor::replaceInstruction(TACBlock* block, TACInstruction* oldInst, std::unique_ptr<TACInstruction> newInst, std::optional<Message> message)
{
    if (message.has_value())
        oldInst->History.push_back(message.value());

    newInst->deadSiblings.preceding = std::move(oldInst->deadSiblings.preceding);
    newInst->History = std::move(oldInst->History);
    newInst->deadSiblings.trailing = std::move(oldInst->deadSiblings.trailing);

    auto& oldInstPtr = *std::find_if(block->Instructions.begin(), block->Instructions.end(), [&oldInst](auto& i) { return i.get() == oldInst; });

    invalidateDataflowAnalyses(block->Function->Metadata);

    switch (oldInst->type)
    {
        case TACType::BRANCH:
        case TACType::JUMP:
            invalidateControlFlowAnalyses(block->Function->Metadata);
            break;
    }

    oldInstPtr = std::move(newInst);
}

void TACEditor::moveInstructionTo(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, std::optional<Message> message)
{
    auto instCopy = cloneTACInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<TACInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    appendInstruction(destBlock, std::move(instCopy), message);
}

void TACEditor::moveInstructionBefore(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, TACInstruction* destInst, std::optional<Message> message)
{
    auto instCopy = cloneTACInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<TACInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionBefore(destBlock, destInst, std::move(instCopy), message);
}

void TACEditor::moveInstructionAfter(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, TACInstruction* destInst, std::optional<Message> message)
{
    auto instCopy = cloneTACInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<TACInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionAfter(destBlock, destInst, std::move(instCopy), message);
}

void TACEditor::deleteInstruction(TACBlock* block, TACInstruction* inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    invalidateDataflowAnalyses(block->Function->Metadata);

    switch (inst->type)
    {
        case TACType::BRANCH:
        case TACType::JUMP:
            invalidateControlFlowAnalyses(block->Function->Metadata);
            break;
    }

    auto instIt = std::find_if(block->Instructions.begin(), block->Instructions.end(), [&inst](auto& i) { return i.get() == inst; });

    cascadeDeletion<TACInstruction>(block->Instructions, instIt - block->Instructions.begin(), &block->LastDeadInstruction);
}

void TACEditor::addEdge(TACBlock* From, TACBlock* To)
{
    if (std::find(From->Children.begin(), From->Children.end(), To) == From->Children.end() && std::find(To->Parents.begin(), To->Parents.end(), From) == To->Parents.end())
    {
        From->Children.push_back(To);
        To->Parents.push_back(From);
    }

    invalidateControlFlowAnalyses(From->Function->Metadata);
    invalidateControlFlowAnalyses(To->Function->Metadata);
}

void TACEditor::removeEdge(TACBlock* From, TACBlock* To)
{
    std::erase(From->Children, To);
    std::erase(To->Parents, From);

    invalidateControlFlowAnalyses(From->Function->Metadata);
    invalidateControlFlowAnalyses(To->Function->Metadata);
}

TACBlock* TACEditor::insertBlockBefore(TACBlock* target, std::optional<Message> message)
{
    auto newBlock = std::make_unique<TACBlock>(target->Function->Blocks.size() + 1, target->Function);

    TACBlock* newBlockPtr = newBlock.get();

    target->Function->Blocks.insert(std::find_if(target->Function->Blocks.begin(), target->Function->Blocks.end(), [&target](auto& b) { return b.get() == target; }), std::move(newBlock));

    if (message.has_value())
    {
        newBlockPtr->History.push_back(message.value());
    }

    invalidateAllAnalyses(target->Function->Metadata);

    return newBlockPtr;
}

void TACEditor::deleteBlock(TACBlock* block, std::optional<Message> message)
{
    if (message.has_value())
        block->History.push_back(message.value());

    for (TACBlock* parent : block->Parents)
        std::erase(parent->Children, block);

    for (TACBlock* child : block->Children)
        std::erase(child->Parents, block);

    auto blockIt = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    cascadeDeletion<TACBlock>(block->Function->Blocks, blockIt - block->Function->Blocks.begin(), &block->Function->LastDeadBlock);

    invalidateAllAnalyses(block->Function->Metadata);
}

void TACEditor::deleteFunction(TAC& TAC, TACFunction* function, std::optional<Message> message)
{
    if (message.has_value())
        function->History.push_back(message.value());

    for (int i = function->Blocks.size() - 1; i >= 0; --i)
    {
        deleteBlock(function->Blocks[i].get());
    }

    auto functionIt = std::find_if(TAC.begin(), TAC.end(), [&function](auto& f) { return f.get() == function; });

    cascadeDeletion<TACFunction>(TAC, functionIt - TAC.begin());
}
