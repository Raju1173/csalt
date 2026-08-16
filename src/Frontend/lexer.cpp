#include "lexer.h"
#include "IRDebugger.h"
#include <cctype>
#include <vector>
#include <print>

void Tokenize(std::string source, TokenStream& tokenStream)
{
    Debugger::AddIR(tokenStream);

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

            std::string lexeme = std::string(source.substr(start, (i - start) + 1));

            if (lexeme == "if")
            {
                tokenStream.push_back({TokenType::IF});
            }

            else if (lexeme == "int")
            {
                tokenStream.push_back({TokenType::INT});
            }

            else if (lexeme == "while")
            {
                tokenStream.push_back({TokenType::WHILE});
            }

            else if (lexeme == "return")
            {
                tokenStream.push_back({TokenType::RETURN});
            }

            else
            {
                tokenStream.push_back({TokenType::IDENTIFIER, lexeme});
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

            std::string value = std::string(source.substr(start, (i - start) + 1));

            tokenStream.push_back({TokenType::NUMBER, value});

            continue;
        }

        switch (current)
        {
            case '+':
                tokenStream.push_back({TokenType::PLUS});
                break;
            case '-':
                tokenStream.push_back({TokenType::MINUS});
                break;
            case '*':
                tokenStream.push_back({TokenType::ASTERISK});
                break;

            case '/':
                if (next == '/')
                {
                    while (i < source.size() && source[i] != '\n')
                    {
                        i++;
                    }
                }

                else if (next == '*')
                {
                    i += 2;

                    while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/'))
                    {
                        i++;
                    }

                    i++;
                }

                else
                {
                    tokenStream.push_back({TokenType::SLASH});
                }

                break;

            case '(':
                tokenStream.push_back({TokenType::LPAREN});
                break;
            case ')':
                tokenStream.push_back({TokenType::RPAREN});
                break;
            case '{':
                tokenStream.push_back({TokenType::LBRACE});
                break;
            case '}':
                tokenStream.push_back({TokenType::RBRACE});
                break;
            case ',':
                tokenStream.push_back({TokenType::COMMA});
                break;
            case ';':
                tokenStream.push_back({TokenType::SEMICOLON});
                break;

            case '=':
                if (next == '=')
                {
                    tokenStream.push_back({TokenType::DOUBLE_EQUAL});
                    i++;
                }

                else
                {
                    tokenStream.push_back({TokenType::EQUAL});
                }

                break;

            case '!':
                if (next == '=')
                {
                    tokenStream.push_back({TokenType::NOT_EQUAL});

                    i++;
                }

                break;

            case '<':
                if (next == '=')
                {
                    tokenStream.push_back({TokenType::LESS_EQUAL});
                    i++;
                }

                else
                {
                    tokenStream.push_back({TokenType::LESS});
                }

                break;

            case '>':
                if (next == '=')
                {
                    tokenStream.push_back({TokenType::GREATER_EQUAL});
                    i++;
                }

                else
                {
                    tokenStream.push_back({TokenType::GREATER});
                }

                break;

            default:
                break;
        }

        continue;
    }

    tokenStream.push_back({TokenType::END});

    Debugger::Notify(Phase::TOK);
}
