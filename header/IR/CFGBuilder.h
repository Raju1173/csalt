#pragma once

#include "lexer.h"
#include "parser.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

struct PhiArgument
{
    size_t SourceID;
    std::string Value;
};

struct PhiNode
{
    std::string variable;

    int version;

    std::vector<PhiArgument> arguments;
};

struct CFGBlock
{
    size_t ID;

    std::vector<PhiNode> PhiNodes;

    std::vector<Node> Statements;

    std::optional<Node> Condition;

    std::optional<CFGBlock*> TransitionNext;
    std::optional<CFGBlock*> TransitionTrue;
    std::optional<CFGBlock*> TransitionFalse;

    std::vector<CFGBlock*> Parents;
};

struct CFGFunction
{
    std::string FunctionName;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<CFGBlock>> Blocks;
};

struct CFGDominatorInfo
{
    bool isValid = false;

    std::unordered_map<CFGBlock*, std::unordered_set<CFGBlock*>> Dominators;
};

struct CFGDominatorTreeInfo
{
    bool isValid = false;

    std::unordered_map<CFGBlock*, std::vector<CFGBlock*>> DominatorTree;
};

struct CFGFrontierInfo
{
    bool isValid = false;

    std::unordered_map<CFGBlock*, std::vector<CFGBlock*>> Frontiers;
};

struct CFGDefBlocksInfo
{
    // DefBlocks is computed on the fly during CFG construction so its guaranteed to be valid initially...
    bool isValid = true;

    std::unordered_map<Token, std::unordered_set<CFGBlock*>> DefBlocks;
};

struct CFGMetaData
{
    CFGDominatorInfo DomInfo;

    CFGDominatorTreeInfo DomTreeInfo;

    CFGFrontierInfo FrontierInfo;

    CFGDefBlocksInfo DefBlocksInfo;
};

class CFG
{
private:
    std::vector<CFGFunction> Functions;

    std::unordered_map<std::string, CFGMetaData> MetaData;

public:
    CFGDefBlocksInfo& getDefBlocksInfo(std::string FuncName)
    {
        return MetaData[FuncName].DefBlocksInfo;
    }

    CFGDominatorInfo& getDominatorInfo(std::string FuncName)
    {
        return MetaData[FuncName].DomInfo;
    }

    CFGDominatorTreeInfo& getDominatorTreeInfo(std::string FuncName)
    {
        return MetaData[FuncName].DomTreeInfo;
    }

    CFGFrontierInfo& getFrontierInfo(std::string FuncName)
    {
        return MetaData[FuncName].FrontierInfo;
    }

    CFGDominatorInfo& computeDominators(CFGFunction& CFGFunc);
    void computeDominators();

    CFGDominatorTreeInfo& computeDominatorTree(CFGFunction& CFGFunc);
    void computeDominatorTree();

    CFGFrontierInfo& computeFrontiers(CFGFunction& CFGFunc, bool ComputeWeakFrontiers = false);
    void computeFrontiers();

    size_t size() const
    {
        return Functions.size();
    }

    CFGFunction& operator[](size_t index)
    {
        return Functions[index];
    }

    const CFGFunction& operator[](size_t index) const
    {
        return Functions[index];
    }

    void push_back(CFGFunction& CFGFunction)
    {
        Functions.push_back(std::move(CFGFunction));
    }

    void push_back(CFGFunction&& CFGFunction)
    {
        Functions.push_back(std::move(CFGFunction));
    }

    CFGFunction& back()
    {
        return Functions.back();
    }

    const CFGFunction& back() const
    {
        return Functions.back();
    }

    auto begin() { return Functions.begin(); }
    auto end() { return Functions.end(); }

    auto begin() const { return Functions.begin(); }
    auto end() const { return Functions.end(); }
};

CFG ConstructCFG(const Node& AST);

void PrintCFG(const CFG& CFG);
