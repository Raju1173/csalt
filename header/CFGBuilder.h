#pragma once

#include "lexer.h"
#include "parser.h"
#include <cstddef>
#include <deque>
#include <memory>
#include <queue>
#include <set>
#include <map>
#include <vector>

struct CFGBlock
{
    size_t ID;

    std::vector<std::unique_ptr<Node>> Statements;

    std::unique_ptr<Node> Condition = nullptr;

    CFGBlock *TransitionNext = nullptr;
    CFGBlock *TransitionTrue = nullptr;
    CFGBlock *TransitionFalse = nullptr;

    std::set<CFGBlock *> Dominators;

    std::vector<CFGBlock *> DominatorTreeChildren;

    std::set<CFGBlock *> Frontiers;

    std::set<Token> ExisitingPhiNodes;

    std::vector<CFGBlock *> Parents;
};

struct CFGFunction
{
    std::string_view FunctionName;

    std::vector<std::unique_ptr<CFGBlock>> Blocks;

    std::map<Token, std::deque<CFGBlock *>> DefBlocks;

    size_t NextBlockID = 1;
};

std::vector<std::unique_ptr<CFGFunction>> constructCFG(const Node &AST);

void printCFG(std::vector<std::unique_ptr<CFGFunction>> CFG);
