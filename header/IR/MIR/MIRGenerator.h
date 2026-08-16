#pragma once

#include "TACGenerator.h"
#include "MIRInstructions.h"
#include "IREditor.h"
#include "MIRAnalyses.h"
#include <memory>
#include <vector>

template<typename IRTypes> class IREditor;

class MIRBlock
{
public:
    size_t ID;

    MIRFunction* Function;

    std::vector<std::unique_ptr<MIRInstruction>> Instructions;

    std::vector<MIRBlock*> Parents;
    std::vector<MIRBlock*> Children;

    std::vector<Message> History;

    DeadSiblings<MIRBlock> deadSiblings;

    std::unique_ptr<MIRInstruction> LastDeadInstruction = nullptr;

    MIRBlock(size_t ID, MIRFunction* function) : ID(ID), Function(function){};

    template<typename IRTypes> friend class IREditor;
};

class MIRFunction
{
public:
    std::string FunctionName;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<MIRBlock>> Blocks;

    int StackFrameSize = 0;

    // gets flipped during MIRgeneration...
    bool IsLeaf = true;

    std::vector<Message> History;

    DeadSiblings<MIRFunction> deadSiblings;

    std::unique_ptr<MIRBlock> LastDeadBlock;

    MIRFunction(std::string name, std::vector<std::string> parameters) : FunctionName(name), Parameters(parameters){};

private:
    MIRMetaData Metadata;

public:
    MIRLivenessInfo& getLivenessInfo();

    template<typename IRTypes> friend class IREditor;
};

using MIR = std::vector<std::unique_ptr<MIRFunction>>;

void GenerateMachineIR(TAC& TAC, MIR& MIR);
