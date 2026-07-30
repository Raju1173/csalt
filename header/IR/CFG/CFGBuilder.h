#pragma once

#include "parser.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct CFGBlock
{
    size_t ID;

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

using CFG = std::vector<CFGFunction>;

CFG ConstructCFG(Node& AST);
