#include "IRDebugger.h"
#include "MIRInstructions.h"
#include "SSAConstructor.h"
#include "TACInstructions.h"
#include <algorithm>
#include <concepts>
#include <vector>
#include "IREditor.h"

template<typename IRTypes> void IREditor<IRTypes>::appendInstruction(Block* block, std::unique_ptr<Instruction> inst, std::optional<Message> msg)
{
    if (msg.has_value())
        inst->History.push_back(msg.value());

    block->Instructions.push_back(std::move(inst));

    invalidateDataFlowAnalyses(block->Function->Metadata);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::addInstructionBefore(Block* block, Instruction* target, std::unique_ptr<Instruction> inst, std::optional<Message> msg)
{
    if (msg.has_value())
        inst->History.push_back(msg.value());

    inst->deadSiblings.preceding.insert(inst->deadSiblings.preceding.begin(), std::make_move_iterator(target->deadSiblings.preceding.begin()), std::make_move_iterator(target->deadSiblings.preceding.end()));

    target->deadSiblings.preceding.clear();

    block->Instructions.insert(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; }), std::move(inst));

    invalidateDataFlowAnalyses(block->Function->Metadata);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::addInstructionAfter(Block* block, Instruction* target, std::unique_ptr<Instruction> inst, std::optional<Message> msg)
{
    if (msg.has_value())
        inst->History.push_back(msg.value());

    inst->deadSiblings.trailing.insert(inst->deadSiblings.trailing.end(), std::make_move_iterator(target->deadSiblings.trailing.begin()), std::make_move_iterator(target->deadSiblings.trailing.end()));

    target->deadSiblings.trailing.clear();

    block->Instructions.insert(std::next(std::find_if(block->Instructions.begin(), block->Instructions.end(), [&target](auto& i) { return i.get() == target; })), std::move(inst));

    invalidateDataFlowAnalyses(block->Function->Metadata);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::replaceInstruction(Block* block, Instruction* oldInst, std::unique_ptr<Instruction> newInst, std::optional<Message> msg)
{
    if (msg.has_value())
        oldInst->History.push_back(msg.value());

    newInst->deadSiblings.preceding = std::move(oldInst->deadSiblings.preceding);
    newInst->History = std::move(oldInst->History);
    newInst->deadSiblings.trailing = std::move(oldInst->deadSiblings.trailing);

    auto& oldInstPtr = *std::find_if(block->Instructions.begin(), block->Instructions.end(), [&oldInst](auto& i) { return i.get() == oldInst; });

    invalidateDataFlowAnalyses(block->Function->Metadata);

    if constexpr (std::same_as<IRTypes, TACTypes>)
    {
        switch (oldInst->type)
        {
            case TACInstType::BRANCH:
            case TACInstType::JUMP:
                invalidateControlFlowAnalyses(block->Function->Metadata);
                break;
        }
    }

    else if constexpr (std::same_as<IRTypes, MIRTypes>)
    {
        switch (oldInst->type)
        {
            case MIRInstType::CJMP:
            case MIRInstType::JMP:
                invalidateControlFlowAnalyses(block->Function->Metadata);
                break;
        }
    }

    oldInstPtr = std::move(newInst);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::cloneInstructionTo(Instruction* inst, Block* destBlock, std::optional<Message> msg)
{
    std::unique_ptr<Instruction> instCopy;

    if constexpr (std::same_as<IRTypes, TACTypes>)
        instCopy = cloneTACInstruction(inst);
    else if constexpr (std::same_as<IRTypes, MIRTypes>)
        instCopy = cloneMIRInstruction(inst);

    appendInstruction(destBlock, std::move(instCopy), msg);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::moveInstructionTo(Block* srcBlock, Instruction* inst, Block* destBlock, std::optional<Message> msg)
{
    std::unique_ptr<Instruction> instCopy;

    if constexpr (std::same_as<IRTypes, TACTypes>)
        instCopy = cloneTACInstruction(inst);
    else if constexpr (std::same_as<IRTypes, MIRTypes>)
        instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<Instruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    appendInstruction(destBlock, std::move(instCopy), msg);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::moveInstructionBefore(Block* srcBlock, Instruction* inst, Block* destBlock, Instruction* destInst, std::optional<Message> msg)
{
    std::unique_ptr<Instruction> instCopy;

    if constexpr (std::same_as<IRTypes, TACTypes>)
        instCopy = cloneTACInstruction(inst);
    else if constexpr (std::same_as<IRTypes, MIRTypes>)
        instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<Instruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionBefore(destBlock, destInst, std::move(instCopy), msg);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::moveInstructionAfter(Block* srcBlock, Instruction* inst, Block* destBlock, Instruction* destInst, std::optional<Message> msg)
{
    std::unique_ptr<Instruction> instCopy;

    if constexpr (std::same_as<IRTypes, TACTypes>)
        instCopy = cloneTACInstruction(inst);
    else if constexpr (std::same_as<IRTypes, MIRTypes>)
        instCopy = cloneMIRInstruction(inst);

    size_t index = std::find_if(srcBlock->Instructions.begin(), srcBlock->Instructions.end(), [&inst](auto& i) { return i.get() == inst; }) - srcBlock->Instructions.begin();

    cascadeDeletion<Instruction>(srcBlock->Instructions, index, &srcBlock->LastDeadInstruction, true);

    addInstructionAfter(destBlock, destInst, std::move(instCopy), msg);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::deleteInstruction(Block* block, Instruction* inst, std::optional<Message> msg)
{
    if (msg.has_value())
        inst->History.push_back(msg.value());

    invalidateDataFlowAnalyses(block->Function->Metadata);

    if constexpr (std::same_as<IRTypes, TACTypes>)
    {
        switch (inst->type)
        {
            case TACInstType::BRANCH:
            case TACInstType::JUMP:
                invalidateControlFlowAnalyses(block->Function->Metadata);
                break;
        }
    }

    else if constexpr (std::same_as<IRTypes, MIRTypes>)
    {
        switch (inst->type)
        {
            case MIRInstType::CJMP:
            case MIRInstType::JMP:
                invalidateControlFlowAnalyses(block->Function->Metadata);
                break;
        }
    }

    auto instIt = std::find_if(block->Instructions.begin(), block->Instructions.end(), [&inst](auto& i) { return i.get() == inst; });

    cascadeDeletion<Instruction>(block->Instructions, instIt - block->Instructions.begin(), &block->LastDeadInstruction);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::addEdge(Block* From, Block* To)
{
    if (std::find(From->Children.begin(), From->Children.end(), To) == From->Children.end() && std::find(To->Parents.begin(), To->Parents.end(), From) == To->Parents.end())
    {
        From->Children.push_back(To);
        To->Parents.push_back(From);
    }

    invalidateControlFlowAnalyses(From->Function->Metadata);
}

template<typename IRTypes> void IREditor<IRTypes>::removeEdge(Block* From, Block* To)
{
    std::erase(From->Children, To);
    std::erase(To->Parents, From);

    invalidateControlFlowAnalyses(From->Function->Metadata);
}

template<typename IRTypes> IRTypes::Block* IREditor<IRTypes>::insertBlockBefore(Block* target, std::optional<Message> msg)
{
    auto newBlock = std::make_unique<Block>(std::ranges::max_element(target->Function->Blocks, {}, &Block::ID)->get()->ID + 1, target->Function);

    Block* newBlockPtr = newBlock.get();

    target->Function->Blocks.insert(std::find_if(target->Function->Blocks.begin(), target->Function->Blocks.end(), [&target](auto& b) { return b.get() == target; }), std::move(newBlock));

    if (msg.has_value())
    {
        newBlockPtr->History.push_back(msg.value());
    }

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);

    return newBlockPtr;
}

template<typename IRTypes> void IREditor<IRTypes>::deleteBlock(Block* block, std::optional<Message> msg)
{
    if (msg.has_value())
        block->History.push_back(msg.value());

    auto blockIt = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    cascadeDeletion<Block>(block->Function->Blocks, blockIt - block->Function->Blocks.begin(), &block->Function->LastDeadBlock);

    invalidateAllAnalyses(block->Function->Metadata);

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::deleteFunction(IR& IR, Function* function, std::optional<Message> msg)
{
    if (msg.has_value())
        function->History.push_back(msg.value());

    for (int i = function->Blocks.size() - 1; i >= 0; --i)
    {
        deleteBlock(function->Blocks[i].get());
    }

    auto functionIt = std::find_if(IR.begin(), IR.end(), [&function](auto& f) { return f.get() == function; });

    cascadeDeletion<TACFunction>(IR, functionIt - IR.begin());

    if (msg.has_value() && (gCompilerOptions[msg->Pass].InteractiveDump || gCompilerOptions[msg->Pass].InteractiveHistoryDump))
        Debugger::Notify(msg->Pass);
}

template<typename IRTypes> void IREditor<IRTypes>::removePhiSource(Block* block, Block* parent) requires std::same_as<IRTypes, TACTypes>
{
    for (auto& inst : block->Instructions)
    {
        if (inst->type != TACInstType::PHI)
            break;

        TACPhi* phi = static_cast<TACPhi*>(inst.get());

        std::erase_if(phi->args, [&](PhiArgument& arg) { return arg.SourceBlock == parent; });

        if (phi->args.size() == 1)
        {
            inst = std::make_unique<TACAssign>(phi->variable, phi->args[0].Value);
        }
    }
}

template<typename IRTypes> void IREditor<IRTypes>::remapPhiSources(Block* block, Block* oldParent, const std::vector<Block*>& newParents) requires std::same_as<IRTypes, TACTypes>
{
    if (newParents.empty())
        return;

    for (auto& inst : block->Instructions)
    {
        if (inst->type != TACInstType::PHI)
            break;

        TACPhi* phi = static_cast<TACPhi*>(inst.get());

        std::vector<PhiArgument> updatedArgs;

        for (PhiArgument& arg : phi->args)
        {
            if (arg.SourceBlock == oldParent)
            {
                for (Block* newParent : newParents)
                    updatedArgs.push_back(PhiArgument{newParent, arg.Value});
            }

            else
                updatedArgs.push_back(arg);
        }

        phi->args = updatedArgs;
    }
}

template class IREditor<TACTypes>;
template class IREditor<MIRTypes>;
