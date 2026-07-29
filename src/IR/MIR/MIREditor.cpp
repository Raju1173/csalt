#include "MIREditor.h"
#include "MIRGenerator.h"
#include <algorithm>
#include <vector>

void MIREditor::appendInstruction(MIRBlock* block, std::unique_ptr<MIRInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    block->Instructions.push_back(std::move(inst));
}

void MIREditor::addInstructionBefore(MIRBlock* block, MIRInstruction* target, std::unique_ptr<MIRInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    inst->deadSiblings.preceding.insert(inst->deadSiblings.preceding.begin(), std::make_move_iterator(target->deadSiblings.preceding.begin()), std::make_move_iterator(target->deadSiblings.preceding.end()));

    target->deadSiblings.preceding.clear();

    block->Instructions.insert(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; }), std::move(inst));
}

void MIREditor::addInstructionAfter(MIRBlock* block, MIRInstruction* target, std::unique_ptr<MIRInstruction> inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    inst->deadSiblings.trailing.insert(inst->deadSiblings.trailing.end(), std::make_move_iterator(target->deadSiblings.trailing.begin()), std::make_move_iterator(target->deadSiblings.trailing.end()));

    target->deadSiblings.trailing.clear();

    block->Instructions.insert(std::next(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; })), std::move(inst));
}

void MIREditor::replaceInstruction(MIRBlock* block, MIRInstruction* oldInst, std::unique_ptr<MIRInstruction> newInst, std::optional<Message> message)
{
    if (message.has_value())
        oldInst->History.push_back(message.value());

    newInst->deadSiblings.preceding = std::move(oldInst->deadSiblings.preceding);
    newInst->History = std::move(oldInst->History);
    newInst->deadSiblings.trailing = std::move(oldInst->deadSiblings.trailing);

    auto& oldInstPtr = *std::find_if(block->Instructions.begin(), block->Instructions.end(), [&oldInst](auto& i) { return i.get() == oldInst; });

    oldInstPtr = std::move(newInst);
}

void MIREditor::moveInstructionTo(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, std::optional<Message> message)
{
    auto instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<MIRInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    appendInstruction(destBlock, std::move(instCopy), message);
}

void MIREditor::moveInstructionBefore(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, MIRInstruction* destInst, std::optional<Message> message)
{
    auto instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<MIRInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionBefore(destBlock, destInst, std::move(instCopy), message);
}

void MIREditor::moveInstructionAfter(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, MIRInstruction* destInst, std::optional<Message> message)
{
    auto instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<MIRInstruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionAfter(destBlock, destInst, std::move(instCopy), message);
}

void MIREditor::deleteInstruction(MIRBlock* block, MIRInstruction* inst, std::optional<Message> message)
{
    if (message.has_value())
        inst->History.push_back(message.value());

    auto instIt = std::find_if(block->Instructions.begin(), block->Instructions.end(), [&inst](auto& i) { return i.get() == inst; });

    cascadeDeletion<MIRInstruction>(block->Instructions, instIt - block->Instructions.begin(), &block->LastDeadInstruction);
}

void MIREditor::addEdge(MIRBlock* From, MIRBlock* To)
{
    if (std::find(From->Children.begin(), From->Children.end(), To) == From->Children.end() && std::find(To->Parents.begin(), To->Parents.end(), From) == To->Parents.end())
    {
        From->Children.push_back(To);
        To->Parents.push_back(From);
    }
}

void MIREditor::removeEdge(MIRBlock* From, MIRBlock* To)
{
    std::erase(From->Children, To);
    std::erase(To->Parents, From);
}

MIRBlock* MIREditor::insertBlockBefore(MIRBlock* target, std::optional<Message> message)
{
    auto newBlock = std::make_unique<MIRBlock>(target->Function->Blocks.size() + 1, target->Function);

    MIRBlock* newBlockPtr = newBlock.get();

    target->Function->Blocks.insert(std::find_if(target->Function->Blocks.begin(), target->Function->Blocks.end(), [&target](auto& b) { return b.get() == target; }), std::move(newBlock));

    if (message.has_value())
    {
        newBlockPtr->History.push_back(message.value());
    }

    return newBlockPtr;
}

void MIREditor::deleteBlock(MIRBlock* block, std::optional<Message> message)
{
    if (message.has_value())
        block->History.push_back(message.value());

    for (MIRBlock* parent : block->Parents)
        std::erase(parent->Children, block);

    for (MIRBlock* child : block->Children)
        std::erase(child->Parents, block);

    auto blockIt = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    cascadeDeletion<MIRBlock>(block->Function->Blocks, blockIt - block->Function->Blocks.begin(), &block->Function->LastDeadBlock);
}

void MIREditor::deleteFunction(MIR& MIR, MIRFunction* function, std::optional<Message> message)
{
    if (message.has_value())
        function->History.push_back(message.value());

    for (int i = function->Blocks.size() - 1; i >= 0; --i)
    {
        deleteBlock(function->Blocks[i].get());
    }

    auto functionIt = std::find_if(MIR.begin(), MIR.end(), [&function](auto& f) { return f.get() == function; });

    cascadeDeletion<MIRFunction>(MIR, functionIt - MIR.begin());
}
