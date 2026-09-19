#pragma once

#include "TACInstructions.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TACBlock;
class TACFunction;

struct TACDominatorInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::unordered_set<TACBlock*>> Dominators;
};

struct TACDominatorTreeInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::vector<TACBlock*>> DominatorTree;
};

struct TACFrontierInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::vector<TACBlock*>> Frontiers;
};

struct TACVarUsesInfo
{
    bool isValid = false;

    std::unordered_map<TACValue, size_t> VarUses;
};

struct TACDefBlocksInfo
{
    bool isValid = false;

    std::unordered_map<TACValue, std::unordered_set<TACBlock*>> DefBlocks;
};

struct TACLoop
{
    TACBlock* Preheader = nullptr;
    TACBlock* Header;
    std::vector<TACBlock*> Latches;

    std::unordered_set<TACBlock*> Blocks;

    TACLoop* parentLoop = nullptr;
    std::vector<std::unique_ptr<TACLoop>> childLoops;
};

struct TACLoopInfo
{
    bool isValid = false;

    std::vector<std::unique_ptr<TACLoop>> LoopForest;
    std::vector<TACLoop*> allLoops;
};

struct TACInductionVariable
{
    TACPhi* phi = nullptr;

    TACValue initVal;
    TACValue stepVal;

    TACBinaryOp* stepInst = nullptr;
};

struct TACInductionVariableInfo
{
    bool isValid = false;

    std::unordered_map<TACLoop*, std::vector<TACInductionVariable>> InductionVariables;
};

struct TACBlockLiveness
{
    std::unordered_set<TACValue> LiveIn;
    std::unordered_set<TACValue> LiveOut;
    int MaxPressure;
};

struct TACLivenessInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, TACBlockLiveness> BlockLiveness;
};

struct TACMetaData
{
    TACDominatorInfo DomInfo;

    TACDominatorTreeInfo DomTreeInfo;

    TACFrontierInfo FrontierInfo;

    TACVarUsesInfo VarUsesInfo;

    TACDefBlocksInfo DefBlocksInfo;

    TACLoopInfo LoopInfo;

    TACInductionVariableInfo InductionVariableInfo;

    TACLivenessInfo LivenessInfo;
};
