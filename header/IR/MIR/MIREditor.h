#pragma once

#include "MIRGenerator.h"

class MIREditor
{
private:
    template<typename T, typename ParentContainer, typename LastDead = typename ParentContainer::value_type> static void cascadeDeletion(ParentContainer& vector, size_t index, LastDead* lastDead = nullptr, bool excludeDeadElementFromCascade = false)
    {
        auto deadElement = std::move(vector[index]);
        auto* deadElementPtr = deadElement.get();

        vector.erase(vector.begin() + index);

        if (index < vector.size())
        {
            vector[index]->deadSiblings.absorbLeftSibling(deadElement);

            if (excludeDeadElementFromCascade)
                std::erase_if(vector[index]->deadSiblings.preceding, [&deadElementPtr](auto& element) { return element.get() == deadElementPtr; });
        }

        else if (index > 0)
        {
            vector[index - 1]->deadSiblings.absorbRightSibling(deadElement);

            if (excludeDeadElementFromCascade)
                std::erase_if(vector[index - 1]->deadSiblings.trailing, [&deadElementPtr](auto& element) { return element.get() == deadElementPtr; });
        }

        else if (lastDead != nullptr)
        {
            if ((*lastDead).get() == nullptr)
            {
                if (!excludeDeadElementFromCascade)
                    *lastDead = std::move(deadElement);
            }

            else
            {
                auto deadPreceding = std::move(deadElement->deadSiblings.preceding);
                auto deadTrailing = std::move(deadElement->deadSiblings.trailing);

                (*lastDead)->deadSiblings.trailing.insert((*lastDead)->deadSiblings.trailing.end(), std::make_move_iterator(deadPreceding.begin()), std::make_move_iterator(deadPreceding.end()));

                if (!excludeDeadElementFromCascade)
                    (*lastDead)->deadSiblings.trailing.push_back(std::move(deadElement));

                (*lastDead)->deadSiblings.trailing.insert((*lastDead)->deadSiblings.trailing.end(), std::make_move_iterator(deadTrailing.begin()), std::make_move_iterator(deadTrailing.end()));
            }
        }
    }

public:
    static void appendInstruction(MIRBlock* block, std::unique_ptr<MIRInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionBefore(MIRBlock* block, MIRInstruction* target, std::unique_ptr<MIRInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionAfter(MIRBlock* block, MIRInstruction* target, std::unique_ptr<MIRInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void replaceInstruction(MIRBlock* block, MIRInstruction* oldInst, std::unique_ptr<MIRInstruction> newInst, std::optional<Message> message = std::nullopt);

    static void moveInstructionTo(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, std::optional<Message> message = std::nullopt);

    static void moveInstructionBefore(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, MIRInstruction* destInst, std::optional<Message> message = std::nullopt);

    static void moveInstructionAfter(MIRBlock* srcBlock, MIRInstruction* inst, MIRBlock* destBlock, MIRInstruction* destInst, std::optional<Message> message = std::nullopt);

    static void deleteInstruction(MIRBlock* B, MIRInstruction* inst, std::optional<Message> message = std::nullopt);

    static void addEdge(MIRBlock* From, MIRBlock* To);

    static void removeEdge(MIRBlock* From, MIRBlock* To);

    static MIRBlock* insertBlockBefore(MIRBlock* target, std::optional<Message> message = std::nullopt);

    static void deleteBlock(MIRBlock* block, std::optional<Message> message = std::nullopt);

    static void deleteFunction(MIR& MIR, MIRFunction* function, std::optional<Message> message = std::nullopt);
};
