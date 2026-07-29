#pragma once

#include "TACGenerator.h"
#include <optional>
#include <span>
#include <vector>

class TACEditor
{
private:
    static void invalidateAllAnalyses(TACMetaData& metadata)
    {
        metadata.VarUsesInfo.isValid = false;
        metadata.DefBlocksInfo.isValid = false;
        metadata.DomInfo.isValid = false;
        metadata.DomTreeInfo.isValid = false;
        metadata.FrontierInfo.isValid = false;
        metadata.LoopInfo.isValid = false;
    }

    static void invalidateControlFlowAnalyses(TACMetaData& metadata)
    {
        metadata.DomInfo.isValid = false;
        metadata.DomTreeInfo.isValid = false;
        metadata.FrontierInfo.isValid = false;
        metadata.LoopInfo.isValid = false;
    }

    static void invalidateDataflowAnalyses(TACMetaData& metadata)
    {
        metadata.VarUsesInfo.isValid = false;
        metadata.DefBlocksInfo.isValid = false;
    }

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
    static void reconstructSSA(TAC& TAC);

    static void appendInstruction(TACBlock* block, std::unique_ptr<TACInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionBefore(TACBlock* block, TACInstruction* target, std::unique_ptr<TACInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionAfter(TACBlock* block, TACInstruction* target, std::unique_ptr<TACInstruction> inst, std::optional<Message> msg = std::nullopt);

    static void replaceInstruction(TACBlock* block, TACInstruction* oldInst, std::unique_ptr<TACInstruction> newInst, std::optional<Message> message = std::nullopt);

    static void moveInstructionTo(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, std::optional<Message> message = std::nullopt);

    static void moveInstructionBefore(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, TACInstruction* destInst, std::optional<Message> message = std::nullopt);

    static void moveInstructionAfter(TACBlock* srcBlock, TACInstruction* inst, TACBlock* destBlock, TACInstruction* destInst, std::optional<Message> message = std::nullopt);

    static void deleteInstruction(TACBlock* B, TACInstruction* inst, std::optional<Message> message = std::nullopt);

    static void addEdge(TACBlock* From, TACBlock* To);

    static void removeEdge(TACBlock* From, TACBlock* To);

    static TACBlock* insertBlockBefore(TACBlock* target, std::optional<Message> message = std::nullopt);

    static void deleteBlock(TACBlock* block, std::optional<Message> message = std::nullopt);

    static void deleteFunction(TAC& TAC, TACFunction* function, std::optional<Message> message = std::nullopt);
};
