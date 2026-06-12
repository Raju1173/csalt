#include "lexer.h"
#include <cctype>
#include <vector>

std::vector<Token> tokenize(std::string_view source)
{
    std::vector<Token> TokenStream;

    for (size_t i = 0; i < source.size(); ++i)
    {
        char current = source[i];
        char next = i + 1 < source.size() ? source[i + 1] : ' ';

        if (std::isspace(current))
            continue;

        if (std::isalpha(current) || current == '_')
        {
            size_t start = i;

            while (i + 1 < source.size() && (std::isalnum(source[i + 1]) || source[i + 1] == '_'))
            {
                i++;
            }

            std::string_view lexeme = source.substr(start, (i - start) + 1);

            if (lexeme == "if")
            {
                TokenStream.push_back({TokenType::IF});
            }

            else if (lexeme == "int")
            {
                TokenStream.push_back({TokenType::INT});
            }

            else if (lexeme == "while")
            {
                TokenStream.push_back({TokenType::WHILE});
            }

            else if (lexeme == "return")
            {
                TokenStream.push_back({TokenType::RETURN});
            }

            else
            {
                TokenStream.push_back({TokenType::IDENTIFIER, lexeme});
            }

            continue;
        }

        if (std::isdigit(current))
        {
            size_t start = i;

            while (i + 1 < source.size() && std::isdigit(source[i + 1]))
            {
                i++;
            }

            std::string_view value = source.substr(start, (i - start) + 1);

            TokenStream.push_back({TokenType::NUMBER, value});

            continue;
        }

        switch (current)
        {
            case '+':
                TokenStream.push_back({TokenType::PLUS});
                break;
            case '-':
                TokenStream.push_back({TokenType::MINUS});
                break;
            case '*':
                TokenStream.push_back({TokenType::ASTERISK});
                break;
            case '/':
                TokenStream.push_back({TokenType::SLASH});
                break;
            case '(':
                TokenStream.push_back({TokenType::LPAREN});
                break;
            case ')':
                TokenStream.push_back({TokenType::RPAREN});
                break;
            case '{':
                TokenStream.push_back({TokenType::LBRACE});
                break;
            case '}':
                TokenStream.push_back({TokenType::RBRACE});
                break;
            case ',':
                TokenStream.push_back({TokenType::COMMA});
                break;
            case ';':
                TokenStream.push_back({TokenType::SEMICOLON});
                break;

            case '=':
                if (next == '=')
                {
                    TokenStream.push_back({TokenType::DOUBLE_EQUAL});
                    i++;
                }

                else
                {
                    TokenStream.push_back({TokenType::EQUAL});
                }

                break;

            case '!':
                if (next == '=')
                {
                    TokenStream.push_back({TokenType::NOT_EQUAL});

                    i++;
                }

                break;

            case '<':
                if (next == '=')
                {
                    TokenStream.push_back({TokenType::LESS_EQUAL});
                    i++;
                }

                else
                {
                    TokenStream.push_back({TokenType::LESS});
                }

                break;

            case '>':
                if (next == '=')
                {
                    TokenStream.push_back({TokenType::GREATER_EQUAL});
                    i++;
                }

                else
                {
                    TokenStream.push_back({TokenType::GREATER});
                }

                break;

            default:
                break;
        }

        continue;
    }

    TokenStream.push_back({TokenType::END});

    return TokenStream;
}
