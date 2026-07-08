#pragma once

#include "lexer.h"
#include <memory>
#include <optional>
#include <vector>

enum class NodeType
{
    PROGRAM,

    VAR,
    FUNCTION,
    PARAMETERS,
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

constexpr std::string_view NodeNames[] = {
    "PROGRAM",
    "VAR",
    "FUNCTION",
    "PARAMETERS",
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

    std::vector<Node> children;
};

Node parse(TokenStream& TokenStream);

void printNode(const Node& node, int depth = 0);

void printAST(const Node& root);
