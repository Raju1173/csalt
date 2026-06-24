#pragma once

#include "parser.h"
#include <cstddef>
#include <memory>
#include <set>
#include <vector>

struct CFGBlock
{
    size_t ID;

    std::vector<Node *> Statements;

    Node *Condition = nullptr;

    CFGBlock *TransitionNext = nullptr;
    CFGBlock *TransitionTrue = nullptr;
    CFGBlock *TransitionFalse = nullptr;

    std::set<CFGBlock *> Dominators;

    std::vector<CFGBlock *> Parents;
};

struct CFGFunction
{
    std::string_view FunctionName;

    std::vector<std::unique_ptr<CFGBlock>> Blocks;

    size_t NextBlockID = 1;
};

std::vector<std::unique_ptr<CFGFunction>> constructCFG(const Node &AST);

void printCFG(std::vector<std::unique_ptr<CFGFunction>> CFG);
