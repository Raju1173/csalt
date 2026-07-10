#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>

size_t NextBlockID = 1;

std::unique_ptr<CFGBlock> constructBlock(Node& ASTBlockNode, CFGFunction& CFGFunc, CFGDefBlocksInfo& DefBlocksInfo, size_t offset = 0, CFGBlock* exitTarget = nullptr)
{
    auto block = std::make_unique<CFGBlock>(NextBlockID++);

    for (size_t i = offset; i < ASTBlockNode.children.size(); i++)
    {
        Node& cur = ASTBlockNode.children[i];

        if (cur.type == NodeType::IF)
        {
            block->Condition = std::move(cur.children[0]);

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, DefBlocksInfo, i + 1, exitTarget);
            CFGBlock* contPtr = continuation.get();
            CFGFunc.Blocks.push_back(std::move(continuation));

            block->TransitionFalse = contPtr;
            contPtr->Parents.push_back(block.get());

            auto trueBranch = constructBlock(cur.children[1], CFGFunc, DefBlocksInfo, 0, contPtr);
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

            auto continuation = constructBlock(ASTBlockNode, CFGFunc, DefBlocksInfo, i + 1, exitTarget);
            CFGBlock* contPtr = continuation.get();

            loopHeader->TransitionFalse = contPtr;
            contPtr->Parents.push_back(loopHeader.get());
            CFGFunc.Blocks.push_back(std::move(continuation));

            auto trueBranch = constructBlock(cur.children[1], CFGFunc, DefBlocksInfo, 0, loopHeader.get());

            loopHeader->TransitionTrue = trueBranch.get();
            trueBranch->Parents.push_back(loopHeader.get());
            CFGFunc.Blocks.push_back(std::move(trueBranch));

            CFGFunc.Blocks.push_back(std::move(loopHeader));

            break;
        }

        else if (cur.type == NodeType::EXPR)
        {
            DefBlocksInfo.DefBlocks[cur.children[0].children[0].token].insert(block.get());
        }

        else if (cur.type == NodeType::VAR)
        {
            DefBlocksInfo.DefBlocks[cur.token].insert(block.get());
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
        CFGFunction newFunc = CFGFunction{AST.children[i].token.lexeme};

        for (auto& arg : AST.children[i].children[0].children)
            newFunc.Parameters.push_back(arg.token.lexeme);

        CFG.push_back(std::move(newFunc));

        auto entryBlock = constructBlock(AST.children[i].children[1], CFG.back(), CFG.getDefBlocksInfo(CFG.back().FunctionName));
        CFG.back().Blocks.push_back(std::move(entryBlock));

        std::reverse(CFG.back().Blocks.begin(), CFG.back().Blocks.end());

        NextBlockID = 1;
    }

    return CFG;
}

CFGDominatorInfo& CFG::computeDominators(CFGFunction& CFGFunc)
{
    CFGDominatorInfo& domInfo = getDominatorInfo(CFGFunc.FunctionName);

    if (!domInfo.isValid)
    {
        domInfo.isValid = true;

        domInfo.Dominators.clear();

        if (CFGFunc.Blocks.empty())
            return domInfo;

        std::unordered_map<CFGBlock*, std::unordered_set<CFGBlock*>>& dominators = domInfo.Dominators;

        auto& blocks = CFGFunc.Blocks;

        CFGBlock* entryBlock = blocks[0].get();

        std::unordered_set<CFGBlock*> universalSet;

        for (auto& block : blocks)
        {
            universalSet.insert(block.get());
        }

        dominators[entryBlock] = {entryBlock};

        for (size_t j = 1; j < blocks.size(); ++j)
        {
            dominators[blocks[j].get()] = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < blocks.size(); j++)
            {
                CFGBlock* curBlock = blocks[j].get();

                if (&curBlock == &entryBlock)
                    continue;

                std::unordered_set<CFGBlock*> newDominators;

                if (!curBlock->Parents.empty())
                {
                    newDominators = domInfo.Dominators[curBlock->Parents[0]];

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (newDominators.empty())
                            break;

                        std::unordered_set<CFGBlock*> currentIntersection;

                        for (CFGBlock* dom : newDominators)
                        {
                            if (dominators[curBlock->Parents[k]].contains(dom))
                            {
                                currentIntersection.insert(dom);
                            }
                        }

                        newDominators = std::move(currentIntersection);
                    }
                }

                newDominators.insert(curBlock);

                if (newDominators != dominators[curBlock])
                {
                    dominators[curBlock] = std::move(newDominators);
                    changed = true;
                }
            }
        }
    }

    return domInfo;
}

void CFG::computeDominators()
{
    for (CFGFunction& Func : Functions)
        computeDominators(Func);
}

CFGDominatorTreeInfo& CFG::computeDominatorTree(CFGFunction& CFGFunc)
{
    CFGDominatorInfo& domInfo = getDominatorInfo(CFGFunc.FunctionName);
    CFGDominatorTreeInfo& domTreeInfo = getDominatorTreeInfo(CFGFunc.FunctionName);

    CFGBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    if (!domTreeInfo.isValid)
    {
        domTreeInfo.isValid = true;

        domTreeInfo.DominatorTree.clear();

        std::unordered_map<CFGBlock*, std::unordered_set<CFGBlock*>>& dominators = domInfo.Dominators;

        std::unordered_map<CFGBlock*, std::vector<CFGBlock*>>& dominatorTree = domTreeInfo.DominatorTree;

        for (auto& block : CFGFunc.Blocks)
        {
            for (CFGBlock* dom : dominators[block.get()])
            {
                if (dom == block.get())
                    continue;

                size_t size = dominators[dom].size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                dominatorTree[nearestDominator].push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }

    return domTreeInfo;
}

void CFG::computeDominatorTree()
{
    for (CFGFunction& Func : Functions)
        computeDominatorTree(Func);
}

//ComputeWeakFrontiers is an experimental argument and has no known use cases, so avoid enabling it unless you understand what it does...

void ComputeBlockFrontiers(CFGBlock* Block, CFGDominatorInfo& DomInfo, CFGDominatorTreeInfo& DomTreeInfo, CFGFrontierInfo& FrontierInfo, bool ComputeWeakFrontiers)
{
    if (Block->TransitionNext.has_value())
    {
        if (!DomInfo.Dominators[Block->TransitionNext.value()].contains(Block))
        {
            FrontierInfo.Frontiers[Block].push_back(Block->TransitionNext.value());
        }
    }

    if (Block->TransitionTrue.has_value())
    {
        if (!DomInfo.Dominators[Block->TransitionTrue.value()].contains(Block))
        {
            FrontierInfo.Frontiers[Block].push_back(Block->TransitionTrue.value());
        }
    }

    if (Block->TransitionFalse.has_value())
    {
        if (!DomInfo.Dominators[Block->TransitionFalse.value()].contains(Block))
        {
            FrontierInfo.Frontiers[Block].push_back(Block->TransitionFalse.value());
        }
    }

    for (CFGBlock* domChild : DomTreeInfo.DominatorTree[Block])
    {
        ComputeBlockFrontiers(domChild, DomInfo, DomTreeInfo, FrontierInfo, ComputeWeakFrontiers);

        for (CFGBlock* childFrontier : FrontierInfo.Frontiers[domChild])
        {
            if ((!DomInfo.Dominators[childFrontier].contains(Block) || childFrontier == Block) || ComputeWeakFrontiers == true)
            {
                FrontierInfo.Frontiers[Block].push_back(childFrontier);
            }
        }
    }
}

CFGFrontierInfo& CFG::computeFrontiers(CFGFunction& CFGFunc, bool ComputeWeakFrontiers)
{
    CFGDominatorInfo& domInfo = getDominatorInfo(CFGFunc.FunctionName);
    CFGDominatorTreeInfo& domTreeInfo = getDominatorTreeInfo(CFGFunc.FunctionName);
    CFGFrontierInfo& frontierInfo = getFrontierInfo(CFGFunc.FunctionName);

    if (!frontierInfo.isValid)
    {
        frontierInfo.isValid = true;

        frontierInfo.Frontiers.clear();

        ComputeBlockFrontiers(CFGFunc.Blocks[0].get(), domInfo, domTreeInfo, frontierInfo, ComputeWeakFrontiers);
    }

    return frontierInfo;
}

void CFG::computeFrontiers()
{
    for (CFGFunction& Func : Functions)
        computeFrontiers(Func);
}

void printBlock(CFGBlock* Block, CFGDominatorInfo DomInfo, CFGDominatorTreeInfo DomTreeInfo, CFGFrontierInfo FrontierInfo)
{
    if (Block->PhiNodes.size() > 0)
    {
        std::print("|    Phi Nodes :\n");

        for (size_t i = 0; i < Block->PhiNodes.size(); i++)
        {
            std::print("|    |    {}{} : {{ ", Block->PhiNodes[i].variable, Block->PhiNodes[i].version);

            for (PhiArgument arg : Block->PhiNodes[i].arguments)
            {
                std::print("{} FROM BLOCK - {}, ", arg.Value, arg.SourceID);
            }

            std::print("}}\n");
        }

        std::print("\n");
    }

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

    std::print("\n|    Dominators : {{ ");

    for (CFGBlock* b : DomInfo.Dominators[Block])
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Dominator Tree Children : {{ ");

    for (CFGBlock* b : DomTreeInfo.DominatorTree[Block])
    {
        std::print("{}, ", b->ID);
    }

    std::print("}}\n");

    std::print("\n|    Frontiers : {{ ");

    for (CFGBlock* b : FrontierInfo.Frontiers[Block])
    {
        std::print("{}, ", b->ID);
    }

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

            printBlock(CFG[i].Blocks[j].get(), CFG.getDominatorInfo(CFG[i].FunctionName), CFG.getDominatorTreeInfo(CFG[i].FunctionName), CFG.getFrontierInfo(CFG[i].FunctionName));
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
