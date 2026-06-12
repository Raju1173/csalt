#pragma once

#include "lexer.h"
#include <memory>
#include <vector>

enum class NodeType
{
    PROGRAM,

    VAR,

    FUNCTION,
    BLOCK,

    IF,
    WHILE,
    RETURN,
    EXPR,

    NUMBER,
    IDENTIFIER,
    BINARY_OP,
    CALL
};

struct Node
{
    NodeType type;
    Token token;
    std::vector<std::unique_ptr<Node>> children;
};

Node parse(const std::vector<Token>& TokenStream);

void printNode(const Node& node, int depth = 0);
