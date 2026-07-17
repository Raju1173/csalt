#include "TACEditor.h"
#include "TACGenerator.h"

void TACEditor::addInstruction(TACBlock* block, std::unique_ptr<TACInstruction> inst, std::optional<Message> message)
{
    block->Instructions.push_back(std::move(inst));

    inst->History.push_back(message.value());
}

void TACEditor::replaceInstruction(TACBlock* block, TACInstruction* oldInst, std::unique_ptr<TACInstruction> newInst, std::optional<Message> message)
{
}

void TACEditor::deleteInstruction(TACBlock* block, TACInstruction* inst, std::optional<Message> message)
{
    inst->dead = true;

    inst->History.push_back(message.value());
}

void TACEditor::addEdge(TACBlock* From, TACBlock* To)
{
    if (std::find(From->Children.begin(), From->Children.end(), To) == From->Children.end())
    {
        From->Children.push_back(To);
        To->Parents.push_back(From);
    }
}

void TACEditor::removeEdge(TACBlock* From, TACBlock* To)
{
    std::erase(From->Children, To);
    std::erase(To->Parents, From);
}

void TACEditor::eraseBlock(TACBlock* block, std::optional<Message> message)
{
    TACFunction* F = block->Function;

    for (TACBlock* parent : block->Parents)
        std::erase(parent->Children, block);
    for (TACBlock* child : block->Children)
        std::erase(child->Parents, block);

    std::erase_if(F->Blocks, [block](const auto& ptr) { return ptr.get() == block; });
}
