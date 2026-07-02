#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <print>
#include <utility>
#include <vector>

size_t NextBlockID = 1;

std::unique_ptr<CFGBlock> constructBlock(Node *ASTBlockNode, CFGFunction *CFGFunc, size_t offset = 0, CFGBlock *exitTarget = nullptr)
{
    auto block = std::make_unique<CFGBlock>(NextBlockID++);

    for (size_t i = offset; i < ASTBlockNode->children.size(); i++)
    {
        Node *cur = ASTBlockNode->children[i].get();

        if (cur->type == NodeType::IF)
        {
            block->Condition = std::move(cur->children[0]);

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, i + 1, exitTarget);
            CFGBlock *contPtr = continuation.get();
            CFGFunc->Blocks.push_back(std::move(continuation));

            block->TransitionFalse = contPtr;
            contPtr->Parents.push_back(block.get());

            auto trueBranch = constructBlock(cur->children[1].get(), CFGFunc, 0, contPtr);
            block->TransitionTrue = trueBranch.get();
            trueBranch->Parents.push_back(block.get());

            CFGFunc->Blocks.push_back(std::move(trueBranch));
            break;
        }

        else if (cur->type == NodeType::WHILE)
        {
            auto loopHeader = std::make_unique<CFGBlock>(NextBlockID++);

            block->TransitionNext = loopHeader.get();
            loopHeader.get()->Parents.push_back(block.get());

            loopHeader->Condition = std::move(cur->children[0]);

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, i + 1, exitTarget);
            CFGBlock *contPtr = continuation.get();

            loopHeader->TransitionFalse = contPtr;
            contPtr->Parents.push_back(loopHeader.get());
            CFGFunc->Blocks.push_back(std::move(continuation));

            auto trueBranch = constructBlock(cur->children[1].get(), CFGFunc, 0, loopHeader.get());

            loopHeader->TransitionTrue = trueBranch.get();
            trueBranch->Parents.push_back(loopHeader.get());
            CFGFunc->Blocks.push_back(std::move(trueBranch));

            CFGFunc->Blocks.push_back(std::move(loopHeader));

            break;
        }

        else if (cur->type == NodeType::EXPR)
        {
            CFGFunc->DefBlocks[cur->children[0]->children[0]->token].insert(block.get());
        }

        else if (cur->type == NodeType::VAR)
        {
            CFGFunc->DefBlocks[cur->token].insert(block.get());
        }

        block->Statements.push_back(std::move(ASTBlockNode->children[i]));

        if (cur->type == NodeType::RETURN)
            break;
    }

    if (block->Condition == nullptr && block->TransitionNext == nullptr)
    {
        block->TransitionNext = exitTarget;

        if (exitTarget != nullptr)
            exitTarget->Parents.push_back(block.get());
    }

    return block;
}

std::vector<std::unique_ptr<CFGFunction>> constructCFG(const Node &AST)
{
    std::vector<std::unique_ptr<CFGFunction>> CFG;

    for (size_t i = 0; i < AST.children.size(); i++)
    {
        auto newFunc = std::make_unique<CFGFunction>(AST.children[i]->token.lexeme);
        CFG.push_back(std::move(newFunc));

        auto entryBlock = constructBlock(AST.children[i]->children[0].get(), CFG.back().get());
        CFG.back()->Blocks.push_back(std::move(entryBlock));

        std::reverse(CFG.back()->Blocks.begin(), CFG.back()->Blocks.end());

        NextBlockID = 1;
    }

    return CFG;
}

void printBlock(CFGBlock *Block)
{
    if (Block->PhiNodes.size() > 0)
    {
        std::print("|    Phi Nodes :\n");

        for (size_t i = 0; i < Block->PhiNodes.size(); i++)
        {
            std::print("|    |    {}{} : {{ ", Block->PhiNodes[i].variable, Block->PhiNodes[i].version);

            for (PhiArgument arg : Block->PhiNodes[i].arguments)
            {
                std::print("{} FROM BLOCK - {}, ", arg.Value, arg.Pred->ID);
            }

            std::print("}}\n");
        }

        std::print("\n");
    }

    std::print("|    Statements :\n");

    for (size_t i = 0; i < Block->Statements.size(); i++)
    {
        printNode(*(Block->Statements[i]), 2);
    }

    if (Block->Condition != nullptr)
    {
        std::print("\n|    Condition :\n");
        printNode(*(Block->Condition), 2);
    }

    if (Block->TransitionNext != nullptr)
        std::print("\n|    Transition Next : Block - {}\n", Block->TransitionNext->ID);

    if (Block->TransitionTrue != nullptr)
        std::print("\n|    Transition True : Block - {}\n", Block->TransitionTrue->ID);

    if (Block->TransitionFalse != nullptr)
        std::print("\n|    Transition False : Block - {}\n", Block->TransitionFalse->ID);

    std::print("\n|    Dominators : {{ ");

    for (CFGBlock *b : Block->Dominators)
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Dominator Tree Children : {{ ");

    for (CFGBlock *b : Block->DominatorTreeChildren)
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Frontiers : {{ ");

    for (CFGBlock *b : Block->Frontiers)
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");
}

void printCFG(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (size_t i = 0; i < CFG.size(); i++)
    {
        std::print("\n# Function - {} :\n", CFG[i]->FunctionName);

        for (size_t j = 0; j < CFG[i]->Blocks.size(); j++)
        {
            std::print("\nBlock - {} :\n\n", CFG[i]->Blocks[j]->ID);
            printBlock(CFG[i]->Blocks[j].get());
        }

        std::print("\nVariable Definitions :\n\n");

        for (auto [token, defBlocks] : CFG[i]->DefBlocks)
        {
            std::print("|    {} : {{ ", token.lexeme);

            for (CFGBlock *b : defBlocks)
            {
                std::print("{}, ", b->ID);
            }

            std::print("}}\n");
        }
    }
}
