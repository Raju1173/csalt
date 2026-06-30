#include "lexer.h"
#include "parser.h"
#include "CFGBuilder.h"
#include <cstddef>
#include <memory>
#include <print>
#include <stack>
#include <utility>
#include <vector>

constexpr int precedence(TokenType op)
{
    switch (op)
    {
        case TokenType::EQUAL:
            return 1;

        case TokenType::DOUBLE_EQUAL:
        case TokenType::NOT_EQUAL:
            return 2;

        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return 3;

        case TokenType::PLUS:
        case TokenType::MINUS:
            return 4;

        case TokenType::ASTERISK:
        case TokenType::SLASH:
            return 5;

        default:
            return 0;
    }
}

Node parse(const std::vector<Token> &TokenStream)
{
    std::stack<std::unique_ptr<Node>> nodeStack;

    nodeStack.push(std::make_unique<Node>(Node{NodeType::PROGRAM, {}, {}}));

    int pos = 0;

    auto pushNode = [&nodeStack](NodeType type, Token val = Token{}, std::vector<std::unique_ptr<Node>> children = {}) {
        nodeStack.push(std::make_unique<Node>(Node{type, val, std::move(children)}));
    };

    auto popAndAttach = [&nodeStack]() {
        auto child = std::move(nodeStack.top());
        nodeStack.pop();
        nodeStack.top()->children.push_back(std::move(child));
    };

    while (TokenStream[pos].type != TokenType::END)
    {
        Token cur = TokenStream[pos];
        Token next = TokenStream[pos + 1];
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
                        continue;
                        break;
                }

                break;

            case NodeType::FUNCTION:
                switch (cur.type)
                {
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
                        continue;
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
                        continue;
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
                        continue;
                        break;
                }

                break;

            case NodeType::EXPR:
                switch (cur.type)
                {
                    case TokenType::RPAREN:
                    case TokenType::COMMA:
                    case TokenType::SEMICOLON: {
                        auto child = std::move(nodeStack.top());
                        nodeStack.pop();

                        if (nodeStack.top()->type != NodeType::BINARY_OP)
                        {
                            nodeStack.top()->children.push_back(std::move(child));
                        }

                        else
                        {
                            nodeStack.top()->children.push_back(std::move(child->children[0]));
                            pos++;
                        }

                        continue;
                    }
                    break;

                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        if (next.type == TokenType::LPAREN)
                        {
                            pushNode(NodeType::CALL, cur, {});
                            pos += 2;
                            continue;
                            break;
                        }

                        else
                        {
                            nodeStack.top()->children.push_back(std::make_unique<Node>(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}));
                            pos++;
                            continue;
                            break;
                        }

                        break;

                    case TokenType::PLUS:
                    case TokenType::MINUS:
                    case TokenType::ASTERISK:
                    case TokenType::SLASH:
                    case TokenType::EQUAL:
                    case TokenType::DOUBLE_EQUAL:
                    case TokenType::NOT_EQUAL:
                    case TokenType::LESS:
                    case TokenType::LESS_EQUAL:
                    case TokenType::GREATER:
                    case TokenType::GREATER_EQUAL:
                        std::vector<std::unique_ptr<Node>> children = std::move(nodeStack.top()->children);
                        nodeStack.top()->children.clear();
                        pushNode(NodeType::BINARY_OP, cur, std::move(children));
                        pos++;
                        continue;
                        break;
                }
                break;

            case NodeType::BINARY_OP:
                switch (cur.type)
                {
                    case TokenType::COMMA:
                    case TokenType::RPAREN:
                    case TokenType::SEMICOLON:
                        popAndAttach();
                        continue;
                        break;

                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        switch (next.type)
                        {
                            case TokenType::COMMA:
                            case TokenType::RPAREN:
                            case TokenType::SEMICOLON:
                                nodeStack.top()->children.push_back(std::make_unique<Node>(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}));
                                popAndAttach();
                                pos++;
                                continue;
                                break;

                            case TokenType::PLUS:
                            case TokenType::MINUS:
                            case TokenType::ASTERISK:
                            case TokenType::SLASH:
                            case TokenType::EQUAL:
                            case TokenType::DOUBLE_EQUAL:
                            case TokenType::NOT_EQUAL:
                            case TokenType::LESS:
                            case TokenType::LESS_EQUAL:
                            case TokenType::GREATER:
                            case TokenType::GREATER_EQUAL:
                                if (precedence(nodeStack.top()->token.type) >= precedence(next.type))
                                {
                                    if (nodeStack.top()->children.size() < 2)
                                    {
                                        nodeStack.top()->children.push_back(std::make_unique<Node>(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}));
                                        popAndAttach();
                                        pos++;
                                        continue;
                                    }

                                    else
                                    {
                                        std::vector<std::unique_ptr<Node>> children;
                                        children.push_back(std::move(nodeStack.top()));
                                        nodeStack.pop();
                                        pushNode(NodeType::BINARY_OP, next, std::move(children));
                                        pos += 2;
                                        continue;
                                    }
                                }

                                else
                                {
                                    std::vector<std::unique_ptr<Node>> children;
                                    children.push_back(std::make_unique<Node>(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}));
                                    pushNode(NodeType::BINARY_OP, next, std::move(children));
                                    pos += 2;
                                    continue;
                                }

                                break;

                            case TokenType::LPAREN:
                                pushNode(NodeType::CALL, cur, {});
                                pos += 2;
                                continue;
                                break;

                            default:
                                nodeStack.top()->children.push_back(std::make_unique<Node>(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}));
                                pos++;
                                continue;
                                break;
                        }

                        break;

                    case TokenType::PLUS:
                    case TokenType::MINUS:
                    case TokenType::ASTERISK:
                    case TokenType::SLASH:
                    case TokenType::EQUAL:
                    case TokenType::DOUBLE_EQUAL:
                    case TokenType::NOT_EQUAL:
                    case TokenType::LESS:
                    case TokenType::LESS_EQUAL:
                    case TokenType::GREATER:
                    case TokenType::GREATER_EQUAL: {
                        TokenType stackOp = nodeStack.top()->token.type;
                        TokenType currentOp = cur.type;

                        if (precedence(stackOp) >= precedence(currentOp))
                        {
                            popAndAttach();
                            continue;
                        }
                        else
                        {
                            auto rightChild = std::move(nodeStack.top()->children.back());
                            nodeStack.top()->children.pop_back();

                            std::vector<std::unique_ptr<Node>> children;
                            children.push_back(std::move(rightChild));

                            pushNode(NodeType::BINARY_OP, cur, std::move(children));
                            pos++;
                            continue;
                        }
                    }

                    break;

                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        pos++;
                        continue;
                        break;
                }

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
                        continue;
                        break;
                }

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
                            pushNode(NodeType::CALL, cur);
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

                    case TokenType::SEMICOLON:
                        if (TokenStream[pos - 1].type == TokenType::INT)
                        {
                            nodeStack.top()->children.push_back(std::make_unique<Node>(Node{NodeType::VAR, cur, {}}));
                            pos += 2;
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
        }

        pos++;
    }

    return std::move(*nodeStack.top());
}

void printNode(const Node &node, int depth)
{
    for (int i = 0; i < depth; i++)
        std::print("|    ");

    if (node.type == NodeType::NUMBER || node.type == NodeType::IDENTIFIER || node.type == NodeType::CALL || node.type == NodeType::BINARY_OP)
        std::print("{}({})\n", NodeNames[std::to_underlying(node.type)], node.type == NodeType::BINARY_OP ? TokenNames[std::to_underlying(node.token.type)] : node.token.lexeme);

    else
        std::print("{}\n", NodeNames[std::to_underlying(node.type)]);

    for (size_t i = 0; i < node.children.size(); i++)
        printNode(*(node.children[i]), depth + 1);
}
