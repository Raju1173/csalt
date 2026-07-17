#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>

size_t NextBlockID = 1;

std::unique_ptr<CFGBlock> constructBlock(Node& ASTBlockNode, CFGFunction& CFGFunc, size_t offset = 0, CFGBlock* exitTarget = nullptr)
{
    auto block = std::make_unique<CFGBlock>(NextBlockID++);

    for (size_t i = offset; i < ASTBlockNode.children.size(); i++)
    {
        Node& cur = ASTBlockNode.children[i];

        if (cur.type == NodeType::IF)
        {
            block->Condition = std::move(cur.children[0]);

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, i + 1, exitTarget);
            CFGBlock* contPtr = continuation.get();
            CFGFunc.Blocks.push_back(std::move(continuation));

            block->TransitionFalse = contPtr;
            contPtr->Parents.push_back(block.get());

            auto trueBranch = constructBlock(cur.children[1], CFGFunc, 0, contPtr);
            block->TransitionTrue = trueBranch.get();
            trueBranch->Parents.push_back(block.get());

            CFGFunc.Blocks.push_back(std::move(trueBranch));
            break;
        }

        else if (cur.type == NodeType::WHILE)
        {
            auto loopHeader = std::make_unique<CFGBlock>(NextBlockID++);

            block->TransitionNext = loopHeader.get();
            loopHeader.get()->Parents.push_back(block.get());

            loopHeader->Condition = std::move(cur.children[0]);

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, i + 1, exitTarget);
            CFGBlock* contPtr = continuation.get();

            loopHeader->TransitionFalse = contPtr;
            contPtr->Parents.push_back(loopHeader.get());
            CFGFunc.Blocks.push_back(std::move(continuation));

            auto trueBranch = constructBlock(cur.children[1], CFGFunc, 0, loopHeader.get());

            loopHeader->TransitionTrue = trueBranch.get();
            trueBranch->Parents.push_back(loopHeader.get());
            CFGFunc.Blocks.push_back(std::move(trueBranch));

            CFGFunc.Blocks.push_back(std::move(loopHeader));

            break;
        }

        block->Statements.push_back(std::move(ASTBlockNode.children[i]));

        if (cur.type == NodeType::RETURN)
            break;
    }

    if (!block->Condition.has_value() && !block->TransitionNext.has_value())
    {
        if (exitTarget != nullptr)
        {
            block->TransitionNext = exitTarget;
            exitTarget->Parents.push_back(block.get());
        }

        else
            block->TransitionNext = std::nullopt;
    }

    return block;
}

CFG ConstructCFG(Node& AST)
{
    CFG CFG;

    for (size_t i = 0; i < AST.children.size(); i++)
    {
        CFGFunction newFunc = CFGFunction{AST.children[i].token.lexeme, {}, {}};

        auto entryBlock = constructBlock(AST.children[i].children[1], newFunc);

        for (Node& param : AST.children[i].children[0].children)
        {
            newFunc.Parameters.push_back(param.token.lexeme);

            // this just adds "param = param" at the start of the entry block to tell the phi insertion algo that parameters also come from the entry block. Now this is not a good way of doing this in general because you have to account for modifiers like const and stuff but csalt doesnt support any of that so...
            entryBlock->Statements.insert(entryBlock->Statements.begin(), Node{NodeType::EXPR, {}, {Node{NodeType::BINARY_OP, Token{TokenType::EQUAL, "="}, {Node{NodeType::IDENTIFIER, Token{TokenType::IDENTIFIER, param.token.lexeme}, {}}, Node{NodeType::IDENTIFIER, Token{TokenType::IDENTIFIER, param.token.lexeme}, {}}}}}});
        }

        CFG.push_back(std::move(newFunc));
        CFG.back().Blocks.push_back(std::move(entryBlock));

        std::reverse(CFG.back().Blocks.begin(), CFG.back().Blocks.end());

        NextBlockID = 1;
    }

    return CFG;
}

void printBlock(CFGBlock* Block)
{
    std::print("|    Statements :\n");

    for (size_t i = 0; i < Block->Statements.size(); i++)
    {
        printNode(Block->Statements[i], 2);
    }

    if (Block->Condition.has_value())
    {
        std::print("\n|    Condition :\n");
        printNode(*(Block->Condition), 2);
    }

    if (Block->TransitionNext.has_value())
        std::print("\n|    Transition Next : Block - {}\n", Block->TransitionNext.value()->ID);

    if (Block->TransitionTrue.has_value())
        std::print("\n|    Transition True : Block - {}\n", Block->TransitionTrue.value()->ID);

    if (Block->TransitionFalse.has_value())
        std::print("\n|    Transition False : Block - {}\n", Block->TransitionFalse.value()->ID);

    std::print("}}\n");
}

void PrintCFG(CFG& CFG)
{
    std::print("------CFG-------\n");

    for (size_t i = 0; i < CFG.size(); i++)
    {
        std::print("\n# Function(");

        for (size_t j = 0; j < CFG[i].Parameters.size(); ++j)
        {
            if (j != 0)
                std::print(", ");

            std::print("{}", CFG[i].Parameters[j]);
        }

        std::print(") - {} :\n", CFG[i].FunctionName);

        for (size_t j = 0; j < CFG[i].Blocks.size(); j++)
        {
            std::print("\nBlock - {} :\n\n", CFG[i].Blocks[j]->ID);

            printBlock(CFG[i].Blocks[j].get());
        }
    }

    std::print("\n");
}
