#pragma once

#include "CFGBuilder.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "TACInstructions.h"
#include "TACAnalyses.h"
#include <vector>
#include <memory>

class TACFunction;

class TACBlock
{
public:
    size_t ID;

    TACFunction* Function;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;

    std::vector<TACBlock*> Parents;
    std::vector<TACBlock*> Children;

    std::vector<Message> History;

    DeadSiblings<TACBlock> deadSiblings;

    std::unique_ptr<TACInstruction> LastDeadInstruction = nullptr;

    TACBlock(size_t ID, TACFunction* function) : ID(ID), Function(function){};

    template<typename IRTypes> friend class IREditor;
};

class TACFunction
{
public:
    std::string Name;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<TACBlock>> Blocks;

    std::vector<Message> History;

    DeadSiblings<TACFunction> deadSiblings;

    std::unique_ptr<TACBlock> LastDeadBlock;

    size_t NextTemp = 0;
    size_t NextBlockID = 1;

    TACFunction(std::string name, std::vector<std::string> parameters) : Name(name), Parameters(parameters){};

private:
    TACMetaData Metadata;

public:
    TACDominatorInfo& getDominatorInfo();

    TACDominatorTreeInfo& getDominatorTreeInfo();

    TACFrontierInfo& getFrontierInfo();

    TACVarUsesInfo& getVarUsesInfo();

    TACDefBlocksInfo& getDefBlocksInfo();

    TACLoopInfo& getLoopInfo();

    template<typename IRTypes> friend class IREditor;
};

using TAC = std::vector<std::unique_ptr<TACFunction>>;

void GenerateTAC(CFG& CFG, TAC& TAC);
