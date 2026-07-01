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

    LBRACE,
    RBRACE,
    LPAREN,
    RPAREN,

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

        "LBRACE",
        "RBRACE",
        "LPAREN",
        "RPAREN",

        "COMMA",
        "SEMICOLON",

        "END"};

struct Token
{
    TokenType type;
    std::string lexeme;

    auto operator<=>(const Token &) const = default;
};

std::vector<Token> tokenize(std::string_view source);
