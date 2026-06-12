#include "parser.h"
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
	if(nodeStack.top()->type == NodeType::RETURN)
	{
	    if(TokenStream[pos].type == TokenType::IDENTIFIER)
	    {
		auto node = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

		nodeStack.push(node);

		continue;
	    }

	    else if(TokenStream[pos].type == TokenType::SEMICOLON)
	    {
		auto child = std::move(nodeStack.top());

		nodeStack.pop();

		nodeStack.top()->children.push_back(std::move(child));

		pos++;

		continue;
	    }
	}

        if(TokenStream[pos].type == TokenType::RETURN)
	{
	    auto node = std::make_unique<Node>(Node{NodeType::RETURN, {}, {}});
	    
	    nodeStack.push(node);
	    
	    pos++;

	    continue;
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
