#include "lexer.h"
#include "parser.h"
#include <cstddef>
#include <memory>
#include <print>
#include <stack>
#include <utility>
#include <vector>

Node parse(const std::vector<Token> &TokenStream)
{
    std::stack<std::unique_ptr<Node>> nodeStack;
    nodeStack.push(std::make_unique<Node>(Node{NodeType::PROGRAM, {}, {}}));

    int pos = 0;

    auto pushNode = [&nodeStack](NodeType type, Token val = Token{})
    {
        nodeStack.push(std::make_unique<Node>(Node{type, val, {}}));
    };

    auto popAndAttach = [&nodeStack]()
    {
        auto child = std::move(nodeStack.top());
        nodeStack.pop();
        nodeStack.top()->children.push_back(std::move(child));
    };

    while (TokenStream[pos].type != TokenType::END)
    {
        Token cur = TokenStream[pos];
        NodeType topType = nodeStack.top()->type;

        switch (topType)
        {
            case NodeType::WHILE:
            case NodeType::IF:
                switch (cur.type)
                {
                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        pos++;
                        continue;
                        break;

                    case TokenType::LBRACE:
                        pushNode(NodeType::BLOCK);
                        pos++;
                        continue;
                        break;

                    case TokenType::RBRACE:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        pos++;
                        break;
                }

                break;

            case NodeType::FUNCTION:
                switch (cur.type)
                {
                    case TokenType::IDENTIFIER:
                        nodeStack.top()->children.push_back(
                            std::make_unique<Node>(
                                Node{NodeType::IDENTIFIER, cur, {}}));
                        pos++;
                        continue;
                        break;

                    case TokenType::LBRACE:
                        pushNode(NodeType::BLOCK);
                        pos++;
                        continue;
                        break;

                    case TokenType::RBRACE:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        pos++;
                        break;
                }

                break;

            case NodeType::CALL:
                switch (cur.type)
                {
                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        pushNode(NodeType::EXPR);
                        continue;
                        break;

                    case TokenType::RPAREN:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        pos++;
                        break;
                }

                break;

            case NodeType::VAR:
                switch (cur.type)
                {
                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        pushNode(NodeType::EXPR);
                        continue;
                        break;

                    case TokenType::SEMICOLON:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        pos++;
                        break;
                }

                break;
            case NodeType::EXPR:

                break;

            case NodeType::RETURN:
                switch (cur.type)
                {
                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        pushNode(NodeType::EXPR);
                        continue;
                        break;

                    case TokenType::SEMICOLON:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        pos++;
                        break;
                }

                break;

            default:
                pos++;
                break;
        }

        switch (cur.type)
        {
            case TokenType::IF:
                pushNode(NodeType::IF);
                pos++;
                continue;
                break;

            case TokenType::WHILE:
                pushNode(NodeType::WHILE);
                pos++;
                continue;
                break;

            case TokenType::RETURN:
                pushNode(NodeType::RETURN);
                pos++;
                continue;
                break;

            case TokenType::IDENTIFIER:
                // pos + 1 without bound check is safe because of the END token...
                switch (TokenStream[pos + 1].type)
                {
                    case TokenType::LPAREN:
                        if (topType == NodeType::PROGRAM)
                        {
                            pushNode(NodeType::FUNCTION, cur);
                            pos += 2;
                            continue;
                        }
                        if (topType == NodeType::BLOCK)
                        {
                            pushNode(NodeType::CALL);
                            pos += 2;
                            continue;
                        }
                        break;

                    case TokenType::EQUAL:
                        if (TokenStream[pos - 1].type == TokenType::INT)
                        {
                            pushNode(NodeType::VAR, cur);
                            pos += 2;
                            continue;
                        }
                        else
                        {
                            pushNode(NodeType::EXPR);
                            continue;
                        }
                        break;

                    default:
                        break;
                }

                break;

            case TokenType::RBRACE:
                if (topType == NodeType::BLOCK)
                {
                    popAndAttach();
                    continue;
                }
                break;

            default:
                pos++;
                break;
        }
    }

    return std::move(*nodeStack.top());
}

void printNode(const Node &node, int depth)
{
    for (int i = 0; i < depth; i++)
        std::print("|    ");

    std::print("{}\n", std::to_underlying(node.type));

    for (size_t i = 0; i < node.children.size(); i++)
        printNode(*(node.children[i]), depth + 1);
}
