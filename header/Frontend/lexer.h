#pragma once

#include <functional>
#include <string_view>
#include <utility>
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

inline std::string TokenTypeToStr(TokenType type)
{
    static_assert(std::to_underlying(TokenType::END) + 1 == 24, "Add/Remove the relevant case from the switch when modifying the TokenType enum!!!");

    switch (type)
    {
        case TokenType::IDENTIFIER:
            return "IDENTIFIER";
        case TokenType::NUMBER:
            return "NUMBER";

        case TokenType::INT:
            return "INT";
        case TokenType::RETURN:
            return "RETURN";
        case TokenType::IF:
            return "IF";
        case TokenType::WHILE:
            return "WHILE";

        case TokenType::PLUS:
            return "PLUS";
        case TokenType::MINUS:
            return "MINUS";
        case TokenType::ASTERISK:
            return "ASTERISK";
        case TokenType::SLASH:
            return "SLASH";

        case TokenType::EQUAL:
            return "EQUAL";
        case TokenType::DOUBLE_EQUAL:
            return "DOUBLE_EQUAL";
        case TokenType::NOT_EQUAL:
            return "NOT_EQUAL";

        case TokenType::LESS:
            return "LESS";
        case TokenType::LESS_EQUAL:
            return "LESS_EQUAL";
        case TokenType::GREATER:
            return "GREATER";
        case TokenType::GREATER_EQUAL:
            return "GREATER_EQUAL";

        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::LPAREN:
            return "LPAREN";
        case TokenType::RPAREN:
            return "RPAREN";

        case TokenType::COMMA:
            return "COMMA";
        case TokenType::SEMICOLON:
            return "SEMICOLON";

        case TokenType::END:
            return "END";
    }
}

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

void Tokenize(std::string source, TokenStream& tokenStream);
