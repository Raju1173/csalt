#include "parser.h"
#include "lexer.h"
#include <cstddef>
#include <memory>
#include <print>
#include <stack>
#include <utility>
#include <vector>

Node parse(const std::vector<Token>& TokenStream)
{
    std::stack<std::unique_ptr<Node>> nodeStack;

    nodeStack.push(std::make_unique<Node>(Node{NodeType::PROGRAM, {}, {}}));

    int pos = 0;

    while (TokenStream[pos].type != TokenType::END)
    {
	Token cur = TokenStream[pos];

	if(nodeStack.top()->type == NodeType::WHILE)
	{
	    if(cur.type == TokenType::LPAREN)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(cur.type == TokenType::LBRACE)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::BLOCK, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(cur.type == TokenType::RBRACE)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}

	if(nodeStack.top()->type == NodeType::IF)
	{
	    if(cur.type == TokenType::LPAREN)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(cur.type == TokenType::LBRACE)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::BLOCK, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(cur.type == TokenType::RBRACE)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}

	if(nodeStack.top()->type == NodeType::FUNCTION)
	{
	    if(cur.type == TokenType::IDENTIFIER)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		continue;
	    }

	    else if(cur.type == TokenType::LBRACE)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::BLOCK, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(cur.type == TokenType::RBRACE)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}

	if(nodeStack.top()->type == NodeType::CALL)
	{
	    if(cur.type == TokenType::IDENTIFIER || cur.type == TokenType::NUMBER)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		continue;
	    }

	    else if(cur.type == TokenType::RPAREN)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}
	
	if(nodeStack.top()->type == NodeType::EXPR)
	{
	    //parse expression...
	}

	if(nodeStack.top()->type == NodeType::RETURN)
	{
	    if(cur.type == TokenType::IDENTIFIER || cur.type == TokenType::NUMBER)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		continue;
	    }

	    else if(cur.type == TokenType::SEMICOLON)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}

	if(cur.type == TokenType::IF)
	{
	    auto node = std::make_unique<Node>(Node{NodeType::IF, {}, {}});

	    nodeStack.push(node);

	    pos++;

	    continue;
	}

	if(cur.type == TokenType::WHILE)
	{
	    auto node = std::make_unique<Node>(Node{NodeType::WHILE, {}, {}});

	    nodeStack.push(node);

	    pos++;

	    continue;
	}

        if(cur.type == TokenType::RETURN)
	{
	    auto node = std::make_unique<Node>(Node{NodeType::RETURN, {}, {}});
	    
	    nodeStack.push(node);
	    
	    pos++;

	    continue;
	}

	// pos + 1 without bound check is safe because a program can never end with an identifier in the supported subset of C...
	if(cur.type == TokenType::IDENTIFIER && TokenStream[pos + 1].type == TokenType::LPAREN)
	{
	    if(nodeStack.top()->type == NodeType::PROGRAM)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::FUNCTION, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }

	    else if(nodeStack.top()->type == NodeType::BLOCK)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::BLOCK, {}, {}});

		nodeStack.push(node);

		pos++;

		continue;
	    }
	}
    }

    return std::move(*nodeStack.top());
}

void printNode(const Node& node, int depth)
{
    for(int i = 0; i < depth; i++)
	std::print("|    ");

    std::print("{}\n", std::to_underlying(node.type));

    for(size_t i = 0; i < node.children.size(); i++)
	printNode(*(node.children[i]), depth + 1);
}
