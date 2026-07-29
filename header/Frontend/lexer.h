#pragma once

#include <functional>
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

constexpr std::string_view TokenNames[] = {
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

    auto operator<=>(const Token&) const = default;
};

namespace std
{
template<> struct hash<Token>
{
    size_t operator()(const Token& t) const noexcept
    {
        return std::hash<std::string_view>{}(t.lexeme) ^ static_cast<size_t>(t.type);
    }
};
}

using TokenStream = std::vector<Token>;

TokenStream Tokenize(std::string_view source);

void PrintTokens(const TokenStream& TokenStream);
