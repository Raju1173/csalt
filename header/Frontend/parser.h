#pragma once

#include "lexer.h"
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
    UNARY_OP
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
    "BINARY_OP",
    "UNARY_OP"};

struct Node
{
    NodeType type;
    Token token;

    std::vector<Node> children;
};

Node Parse(TokenStream& TokenStream);

void printNode(const Node& node, int depth = 0);

void PrintAST(const Node& root);
