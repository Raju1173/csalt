#include "Globals.h"
#include "IRDebugger.h"
#include "lexer.h"
#include "parser.h"
#include <cstddef>
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

// This abomination of a parser was basically my attempt to understand recursive descent and pratt parsing at a deeper level by merging them together into a single loop with a stack (I dont hate clean code)...

// To understand how it works, refer to 'parser.md' inside the docs directory...

void Parse(TokenStream& TokenStream, Node& AST)
{
    Debugger::AddIR(AST);

    std::stack<Node> nodeStack;

    nodeStack.push(Node{Node{NodeType::PROGRAM, {}, {}}});

    size_t pos = 0;

    auto pushNode = [&nodeStack](NodeType type, Token val = Token{}, std::vector<Node> children = {}) {
        nodeStack.push(
            Node{Node{type, val, children}});
    };

    auto popAndAttach = [&nodeStack]() {
        Node child = std::move(nodeStack.top());
        nodeStack.pop();
        nodeStack.top().children.push_back(child);
    };

    while (TokenStream[pos].type != TokenType::END)
    {
        Token cur = TokenStream[pos];
        Token next = TokenStream[pos + 1]; // pos + 1 without bound check is safe because of the END token...
        NodeType topType = nodeStack.top().type;

        switch (topType)
        {
            case NodeType::PROGRAM:
                if (cur.type == TokenType::IDENTIFIER)
                {
                    if (next.type == TokenType::LPAREN)
                    {
                        pushNode(NodeType::FUNCTION, cur);
                        pos++;
                        continue;
                    }
                }

                else
                {
                    pos++;
                    continue;
                }
                break;


            case NodeType::BLOCK:
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
                        switch (next.type)
                        {
                            case TokenType::LPAREN:
                                pushNode(NodeType::CALL, cur);
                                pos += 2;
                                continue;
                                break;

                            case TokenType::EQUAL:
                                // pos - 1 without bound check is safe because in order for the BLOCK node to be at the top of the stack, the node below it must have consumed an LBRACE token...
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
                                    nodeStack.top().children.push_back(Node{NodeType::VAR, cur, {}});
                                    pos += 2;
                                    continue;
                                }
                                break;

                            default:
                                break;
                        }

                        break;

                    case TokenType::RBRACE:
                        popAndAttach();
                        continue;
                        break;

                    default:
                        pos++;
                        continue;
                        break;
                }
                break;

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
                    case TokenType::LPAREN:
                        pushNode(NodeType::PARAMETERS);
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

            case NodeType::PARAMETERS:
                switch (cur.type)
                {
                    case TokenType::IDENTIFIER:
                        nodeStack.top().children.push_back(Node{NodeType::IDENTIFIER, cur, {}});
                        pos++;
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

            case NodeType::CALL:
                switch (cur.type)
                {
                    case TokenType::MINUS:
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
                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        continue;
                        break;

                    case TokenType::MINUS:
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
                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        pos++;
                        continue;
                        break;

                    case TokenType::RPAREN:
                    case TokenType::COMMA:
                    case TokenType::SEMICOLON:
                        {
                            Node child = std::move(nodeStack.top());
                            nodeStack.pop();

                            if (nodeStack.top().type == NodeType::BINARY_OP || nodeStack.top().type == NodeType::EXPR)
                            {
                                nodeStack.top().children.push_back(child.children[0]);
                                pos++;
                            }

                            else if (nodeStack.top().type == NodeType::UNARY_OP)
                            {
                                nodeStack.top().children.push_back(child.children[0]);
                            }

                            else
                            {
                                nodeStack.top().children.push_back(child);
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
                            nodeStack.top().children.push_back(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}});
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
                        if (nodeStack.top().children.size() > 0)
                        {
                            std::vector<Node> children = nodeStack.top().children;
                            nodeStack.top().children.clear();
                            pushNode(NodeType::BINARY_OP, cur, std::move(children));
                            pos++;
                            continue;
                        }

                        else
                        {
                            pushNode(NodeType::UNARY_OP, cur, {});
                            pos++;
                            continue;
                        }
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
                                nodeStack.top().children.push_back(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}});
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
                                if (precedence(nodeStack.top().token.type) >= precedence(next.type))
                                {
                                    if (nodeStack.top().children.size() < 2)
                                    {
                                        nodeStack.top().children.push_back(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}});
                                        popAndAttach();
                                        pos++;
                                        continue;
                                    }

                                    else
                                    {
                                        Node prevTop = nodeStack.top();
                                        nodeStack.pop();
                                        pushNode(NodeType::BINARY_OP, next, {prevTop});
                                        pos += 2;
                                        continue;
                                    }
                                }

                                else
                                {
                                    pushNode(NodeType::BINARY_OP, next, {Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}}});
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
                                nodeStack.top().children.push_back(Node{cur.type == TokenType::NUMBER ? NodeType::NUMBER : NodeType::IDENTIFIER, cur, {}});
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
                        {
                            if (TokenStream[pos - 1].type != TokenType::IDENTIFIER && TokenStream[pos - 1].type != TokenType::NUMBER && nodeStack.top().children.size() < 2)
                            {
                                pushNode(NodeType::UNARY_OP, cur, {});
                                pos++;
                                continue;
                            }

                            else
                            {
                                if (precedence(nodeStack.top().token.type) >= precedence(cur.type))
                                {
                                    popAndAttach();
                                    continue;
                                }

                                else
                                {
                                    Node rightChild = std::move(nodeStack.top().children.back());
                                    nodeStack.top().children.pop_back();

                                    pushNode(NodeType::BINARY_OP, cur, {rightChild});
                                    pos++;
                                    continue;
                                }
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

            case NodeType::UNARY_OP:
                switch (cur.type)
                {
                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        pos++;
                        continue;
                        break;

                    case TokenType::NUMBER:
                    case TokenType::IDENTIFIER:
                        {
                            if (next.type == TokenType::LPAREN)
                            {
                                pushNode(NodeType::CALL, cur, {});
                                pos += 2;
                                continue;
                            }

                            else
                            {
                                if (cur.type == TokenType::IDENTIFIER)
                                {
                                    Node child = Node{NodeType::IDENTIFIER, cur, {}};
                                    nodeStack.top().children.push_back(Node{child});
                                    popAndAttach();
                                    pos++;
                                    continue;
                                }

                                else
                                {
                                    Node child = Node{NodeType::NUMBER, Token{TokenType::NUMBER, "-" + cur.lexeme}, {}};
                                    nodeStack.pop();
                                    nodeStack.top().children.push_back(Node{child});
                                    pos++;
                                    continue;
                                }
                            }
                        }
                        break;

                    case TokenType::RPAREN:
                        popAndAttach();
                        pos++;
                        continue;
                        break;

                    default:
                        if (nodeStack.top().children.size() == 1)
                        {
                            popAndAttach();
                            continue;
                        }
                        break;
                }
                break;

            case NodeType::RETURN:
                switch (cur.type)
                {
                    case TokenType::LPAREN:
                        pushNode(NodeType::EXPR);
                        continue;
                        break;

                    case TokenType::MINUS:
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
    }

    AST = nodeStack.top();

    Debugger::Notify(Phase::AST);
}
