#pragma once

#include <string_view>
#include <vector>

enum class TokenType
{
    IDENTIFIER,
    NUMBER,

    INT,
    RETURN,
    IF,
    WHILE,

    PLUS,
    MINUS,
    ASTERISK,
    SLASH,

    EQUAL,
    DOUBLE_EQUAL,
    NOT_EQUAL,

    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,

    COMMA,
    SEMICOLON,

    END
};

constexpr std::string_view TokenNames[] =
    {
        "IDENTIFIER",
        "NUMBER",

        "INT",
        "RETURN",
        "IF",
        "WHILE",

        "PLUS",
        "MINUS",
        "ASTERISK",
        "SLASH",

        "EQUAL",
        "DOUBLE_EQUAL",
        "NOT_EQUAL",

        "LESS",
        "LESS_EQUAL",
        "GREATER",
        "GREATER_EQUAL",

        "LPAREN",
        "RPAREN",
        "LBRACE",
        "RBRACE",

        "COMMA",
        "SEMICOLON",

        "END"};

struct Token
{
    TokenType type;
    std::string_view lexeme;
};

std::vector<Token> tokenize(std::string_view source);
