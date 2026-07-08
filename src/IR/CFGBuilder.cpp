#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <print>
#include <utility>
#include <vector>

size_t NextBlockID = 1;

CFGBlock constructBlock(CFG& CFG, const Node& ASTBlockNode, CFGFunction& CFGFunc, size_t offset = 0, std::optional<CFGBlock> exitTarget = std::nullopt)
{
    CFGBlock block = CFGBlock{NextBlockID++};

    for (size_t i = offset; i < ASTBlockNode.children.size(); i++)
    {
        const Node& cur = ASTBlockNode.children[i];

        if (cur.type == NodeType::IF)
        {
            block.Condition = cur.children[0];

            CFGBlock continuation = constructBlock(CFG, ASTBlockNode, CFGFunc, i + 1, exitTarget);

            CFGFunc.Blocks.push_back(continuation);

            block.TransitionFalse = &continuation;
            continuation.Parents.push_back(&block);

            CFGBlock trueBranch = constructBlock(CFG, cur.children[1], CFGFunc, 0, continuation);
            block.TransitionTrue = &trueBranch;
            trueBranch.Parents.push_back(&block);

            CFGFunc.Blocks.push_back(std::move(trueBranch));
            break;
        }

        else if (cur.type == NodeType::WHILE)
        {
            CFGBlock loopHeader = CFGBlock{NextBlockID++};

            block.TransitionNext = &loopHeader;
            loopHeader.Parents.push_back(&block);

            loopHeader.Condition = std::move(cur.children[0]);

            CFGBlock continuation = constructBlock(CFG, ASTBlockNode, CFGFunc, i + 1, exitTarget);

            loopHeader.TransitionFalse = &continuation;
            continuation.Parents.push_back(&loopHeader);
            CFGFunc.Blocks.push_back(std::move(continuation));

            CFGBlock trueBranch = constructBlock(CFG, cur.children[1], CFGFunc, 0, loopHeader);

            loopHeader.TransitionTrue = &trueBranch;
            trueBranch.Parents.push_back(&loopHeader);
            CFGFunc.Blocks.push_back(trueBranch);

            CFGFunc.Blocks.push_back(std::move(loopHeader));

            break;
        }

        else if (cur.type == NodeType::EXPR)
        {
            CFG.getDefBlocksInfo(CFGFunc.FunctionName).DefBlocks[cur.children[0].children[0].token].insert(&block);
        }

        else if (cur.type == NodeType::VAR)
        {
            CFG.getDefBlocksInfo(CFGFunc.FunctionName).DefBlocks[cur.token].insert(&block);
        }

        block.Statements.push_back(ASTBlockNode.children[i]);

        if (cur.type == NodeType::RETURN)
            break;
    }

    if (block.Condition.has_value() && block.TransitionNext.has_value())
    {
        block.TransitionNext = &(exitTarget.value());

        if (exitTarget.has_value())
            exitTarget.value().Parents.push_back(&block);
    }

    return block;
}

CFG constructCFG(const Node& AST)
{
    CFG CFG;

    for (size_t i = 0; i < AST.children.size(); i++)
    {
        CFGFunction newFunc = CFGFunction{AST.children[i].token.lexeme};

        for (const Node& arg : AST.children[i].children[0].children)
            newFunc.Parameters.push_back(arg.token.lexeme);

        CFG.push_back(newFunc);

        CFGBlock entryBlock = constructBlock(CFG, AST.children[i].children[1], CFG.back());
        CFG.back().Blocks.push_back(std::move(entryBlock));

        std::reverse(CFG.back().Blocks.begin(), CFG.back().Blocks.end());

        NextBlockID = 1;
    }

    return CFG;
}

void printBlock(CFGBlock& Block, CFGDominatorInfo DomInfo, CFGDominatorTreeInfo DomTreeInfo, CFGFrontierInfo FrontierInfo)
{
    if (Block.PhiNodes.size() > 0)
    {
        std::print("|    Phi Nodes :\n");

        for (size_t i = 0; i < Block.PhiNodes.size(); i++)
        {
            std::print("|    |    {}{} : {{ ", Block.PhiNodes[i].variable, Block.PhiNodes[i].version);

            for (PhiArgument arg : Block.PhiNodes[i].arguments)
            {
                std::print("{} FROM BLOCK - {}, ", arg.Value, arg.SourceID);
            }

            std::print("}}\n");
        }

        std::print("\n");
    }

    std::print("|    Statements :\n");

    for (size_t i = 0; i < Block.Statements.size(); i++)
    {
        printNode(Block.Statements[i], 2);
    }

    if (Block.Condition.has_value())
    {
        std::print("\n|    Condition :\n");
        printNode(*(Block.Condition), 2);
    }

    if (Block.TransitionNext.has_value())
        std::print("\n|    Transition Next : Block - {}\n", Block.TransitionNext.value()->ID);

    if (Block.TransitionTrue.has_value())
        std::print("\n|    Transition True : Block - {}\n", Block.TransitionTrue.value()->ID);

    if (Block.TransitionFalse.has_value())
        std::print("\n|    Transition False : Block - {}\n", Block.TransitionFalse.value()->ID);

    std::print("\n|    Dominators : {{ ");

    for (CFGBlock* b : DomInfo.Dominators[&Block])
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Dominator Tree Children : {{ ");

    for (CFGBlock* b : DomTreeInfo.DominatorTree[&Block])
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Frontiers : {{ ");

    for (CFGBlock* b : FrontierInfo.Frontiers[&Block])
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");
}

void printCFG(CFG& CFG)
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
            std::print("\nBlock - {} :\n\n", CFG[i].Blocks[j].ID);

            printBlock(CFG[i].Blocks[j], CFG.getDominatorInfo(CFG[i].FunctionName), CFG.getDominatorTreeInfo(CFG[i].FunctionName), CFG.getFrontierInfo(CFG[i].FunctionName));
        }

        std::print("\nVariable Definitions :\n\n");

        for (auto [token, defBlocks] : CFG.getDefBlocksInfo(CFG[i].FunctionName).DefBlocks)
        {
            std::print("|    {} : {{ ", token.lexeme);

            for (CFGBlock* b : defBlocks)
            {
                std::print("{}, ", b->ID);
            }

            std::print("}}\n");
        }
    }

    std::print("\n");
}
