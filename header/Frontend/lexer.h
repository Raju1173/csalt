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

class TokenStream
{
private:
    std::vector<Token> Tokens;

public:
    TokenStream(std::vector<Token>& tokenStream) : Tokens(tokenStream){};

    std::vector<Token>& getTokens()
    {
        return Tokens;
    }

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

TokenStream Tokenize(std::string_view source);

void PrintTokens(const TokenStream& TokenStream);
