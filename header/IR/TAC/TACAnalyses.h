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
    TACBlock* Header;
    std::vector<TACBlock*> Latches;
    std::unordered_set<TACBlock*> Blocks;
};

struct TACLoopInfo
{
    bool isValid = false;

    std::vector<TACLoop> Loops;
};

struct TACMetaData
{
    TACDominatorInfo DomInfo;

    TACDominatorTreeInfo DomTreeInfo;

    TACFrontierInfo FrontierInfo;

    TACVarUsesInfo VarUsesInfo;

    TACDefBlocksInfo DefBlocksInfo;

    TACLoopInfo LoopInfo;
};
