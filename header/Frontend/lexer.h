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

class TokenStream
{
public:
    std::vector<Token> Tokens;

    TokenStream(std::vector<Token>& tokenStream) : Tokens(tokenStream){};

    size_t size() const
    {
        return Tokens.size();
    }

    Token& operator[](size_t index)
    {
        return Tokens[index];
    }

    const Token& operator[](size_t index) const
    {
        return Tokens[index];
    }

    auto begin() { return Tokens.begin(); }
    auto end() { return Tokens.end(); }

    auto begin() const { return Tokens.begin(); }
    auto end() const { return Tokens.end(); }
};

TokenStream tokenize(std::string_view source);

void printTokens(const TokenStream& TokenStream);
