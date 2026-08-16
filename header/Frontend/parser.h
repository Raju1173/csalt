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
    UNARY_OP,

    COUNT
};

inline std::string NodeTypeToStr(NodeType type)
{
    static_assert(std::to_underlying(NodeType::COUNT) == 14, "Add/Remove the relevant case from the switch when modifying the NodeType enum!!!");

    switch (type)
    {
        case NodeType::PROGRAM:
            return "PROGRAM";

        case NodeType::VAR:
            return "VAR";
        case NodeType::FUNCTION:
            return "FUNCTION";
        case NodeType::PARAMETERS:
            return "PARAMETERS";
        case NodeType::CALL:
            return "CALL";

        case NodeType::IF:
            return "IF";
        case NodeType::WHILE:
            return "WHILE";
        case NodeType::RETURN:
            return "RETURN";
        case NodeType::BLOCK:
            return "BLOCK";

        case NodeType::EXPR:
            return "EXPR";
        case NodeType::IDENTIFIER:
            return "IDENTIFIER";
        case NodeType::NUMBER:
            return "NUMBER";

        case NodeType::BINARY_OP:
            return "BINARY_OP";
        case NodeType::UNARY_OP:
            return "UNARY_OP";
    }
}


struct Node
{
    NodeType type;
    Token token;

    std::vector<Node> children;
};

void Parse(TokenStream& TokenStream, Node& AST);
