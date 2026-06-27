#pragma once

#include "lexer.h"
#include <memory>
#include <vector>

enum class NodeType
{
    PROGRAM,

    VAR,
    FUNCTION,
    CALL,

    IF,
    WHILE,
    RETURN,
    BLOCK,

    EXPR,
    IDENTIFIER,
    NUMBER,
    BINARY_OP,
};

constexpr std::string_view NodeNames[] =
    {
        "PROGRAM",

        "VAR",
        "FUNCTION",
        "CALL",

        "IF",
        "WHILE",
        "RETURN",
        "BLOCK",

        "EXPR",
        "IDENTIFIER",
        "NUMBER",
        "BINARY_OP"};

struct CFGBlock;

struct Node
{
    NodeType type;
    Token token;
    std::vector<std::unique_ptr<Node>> children;

    CFGBlock *source = nullptr;
};

Node parse(const std::vector<Token> &TokenStream);

void printNode(const Node &node, int depth = 0);
