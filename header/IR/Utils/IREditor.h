#pragma once

#include "TACAnalyses.h"
#include "MIRAnalyses.h"
#include <concepts>
#include <optional>

class TACFunction;
class TACBlock;
class TACInstruction;

class MIRFunction;
class MIRBlock;
class MIRInstruction;

using TAC = std::vector<std::unique_ptr<TACFunction>>;
using MIR = std::vector<std::unique_ptr<MIRFunction>>;

struct TACTypes
{
    using IR = TAC;
    using Function = TACFunction;
    using Block = TACBlock;
    using Instruction = TACInstruction;
};

struct MIRTypes
{
    using IR = MIR;
    using Function = MIRFunction;
    using Block = MIRBlock;
    using Instruction = MIRInstruction;
};

template<typename IRTypes> class IREditor
{
public:
    using IR = typename IRTypes::IR;
    using Function = typename IRTypes::Function;
    using Block = typename IRTypes::Block;
    using Instruction = typename IRTypes::Instruction;

private:
    static void invalidateAllAnalyses(TACMetaData& metadata)
    {
        metadata.VarUsesInfo.isValid = false;
        metadata.DefBlocksInfo.isValid = false;
        metadata.DomInfo.isValid = false;
        metadata.DomTreeInfo.isValid = false;
        metadata.FrontierInfo.isValid = false;
        metadata.LoopInfo.isValid = false;
        metadata.InductionVariableInfo.isValid = false;
    }

    static void invalidateControlFlowAnalyses(TACMetaData& metadata)
    {
        metadata.DomInfo.isValid = false;
        metadata.DomTreeInfo.isValid = false;
        metadata.FrontierInfo.isValid = false;
        metadata.LoopInfo.isValid = false;
        metadata.InductionVariableInfo.isValid = false;
    }

    static void invalidateDataFlowAnalyses(TACMetaData& metadata)
    {
        metadata.VarUsesInfo.isValid = false;
        metadata.DefBlocksInfo.isValid = false;
        metadata.InductionVariableInfo.isValid = false;
    }

    static void invalidateAllAnalyses(MIRMetaData& metadata)
    {
        metadata.LivenessInfo.isValid = false;
    }

    static void invalidateControlFlowAnalyses(MIRMetaData& metadata)
    {
        metadata.LivenessInfo.isValid = false;
    }

    static void invalidateDataFlowAnalyses(MIRMetaData& metadata)
    {
        metadata.LivenessInfo.isValid = false;
    }

    template<typename T, typename ParentContainer, typename LastDead = typename ParentContainer::value_type> static void cascadeDeletion(ParentContainer& vector, size_t index, LastDead* lastDead = nullptr, bool excludeDeadElementFromCascade = false)
    {
        auto deadElement = std::move(vector[index]);
        auto* deadElementPtr = deadElement.get();

        vector.erase(vector.begin() + index);

        if (index < vector.size())
        {
            vector[index]->deadSiblings.absorbLeftSiblingCorpse(deadElement);

            if (excludeDeadElementFromCascade)
            {
                std::erase_if(vector[index]->deadSiblings.preceding, [&deadElementPtr](auto& element) { return element.get() == deadElementPtr; });
            }
        }
        else if (index > 0)
        {
            vector[index - 1]->deadSiblings.absorbRightSiblingCorpse(deadElement);

            if (excludeDeadElementFromCascade)
            {
                std::erase_if(vector[index - 1]->deadSiblings.trailing, [&deadElementPtr](auto& element) { return element.get() == deadElementPtr; });
            }
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
                {
                    (*lastDead)->deadSiblings.trailing.push_back(std::move(deadElement));
                }

                (*lastDead)->deadSiblings.trailing.insert((*lastDead)->deadSiblings.trailing.end(), std::make_move_iterator(deadTrailing.begin()), std::make_move_iterator(deadTrailing.end()));
            }
        }
    }

public:
    static void appendInstruction(Block* block, std::unique_ptr<Instruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionBefore(Block* block, Instruction* target, std::unique_ptr<Instruction> inst, std::optional<Message> msg = std::nullopt);

    static void addInstructionAfter(Block* block, Instruction* target, std::unique_ptr<Instruction> inst, std::optional<Message> msg = std::nullopt);

    static void replaceInstruction(Block* block, Instruction* oldInst, std::unique_ptr<Instruction> newInst, std::optional<Message> msg = std::nullopt);

    static void cloneInstructionTo(Instruction* inst, Block* destBlock, std::optional<Message> msg = std::nullopt);

    static void moveInstructionTo(Block* srcBlock, Instruction* inst, Block* destBlock, std::optional<Message> message = std::nullopt);

    static void moveInstructionBefore(Block* srcBlock, Instruction* inst, Block* destBlock, Instruction* destInst, std::optional<Message> msg = std::nullopt);

    static void moveInstructionAfter(Block* srcBlock, Instruction* inst, Block* destBlock, Instruction* destInst, std::optional<Message> msg = std::nullopt);

    static void deleteInstruction(Block* block, Instruction* inst, std::optional<Message> msg = std::nullopt);

    static void addEdge(Block* From, Block* To);

    static void removeEdge(Block* From, Block* To);

    static Block* insertBlockBefore(Block* target, std::optional<Message> msg = std::nullopt);

    static Block* insertBlockAfter(Block* target, std::optional<Message> msg = std::nullopt);

    static Block* cloneBlockBefore(Block* source, Block* target, std::optional<Message> msg = std::nullopt);

    static void deleteBlock(Block* block, std::optional<Message> msg = std::nullopt);

    static void deleteFunction(IR& IR, Function* function, std::optional<Message> msg = std::nullopt);

    static void removePhiSource(TACBlock* block, TACBlock* parent) requires std::same_as<IRTypes, TACTypes>;

    static void remapPhiSources(TACBlock* block, TACBlock* oldParent, const std::vector<TACBlock*>& newParents) requires std::same_as<IRTypes, TACTypes>;

    static void redirectPhiSourcesInto(TACBlock* targetBlock, TACBlock* intermediateBlock, std::vector<TACBlock*>& redirectedParents) requires std::same_as<IRTypes, TACTypes>;
};
