#include "SSAConstructor.h"
#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <print>
#include <unordered_set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

void ComputeDominators(CFG& CFG)
{
    for (const CFGFunction& CFGFunc : CFG)
    {
        if (CFGFunc.Blocks.empty())
            continue;

        std::unordered_map<CFGBlock*, std::unordered_set<CFGBlock*>>& Dominators = CFG.getDominatorInfo(CFGFunc.FunctionName).Dominators;

        std::vector<CFGBlock> Blocks = CFGFunc.Blocks;

        CFGBlock& entryBlock = Blocks[0];

        std::unordered_set<CFGBlock*> universalSet;

        for (CFGBlock& block : Blocks)
        {
            universalSet.insert(&block);
        }

        Dominators[&entryBlock] = {&entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            Dominators[&Blocks[j]] = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                CFGBlock& curBlock = Blocks[j];

                if (&curBlock == &entryBlock)
                    continue;

                std::unordered_set<CFGBlock*> NewDominators;

                if (!curBlock.Parents.empty())
                {
                    NewDominators = CFG.getDominatorInfo(CFGFunc.FunctionName).Dominators[curBlock.Parents[0]];

                    for (size_t k = 1; k < curBlock.Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::unordered_set<CFGBlock*> currentIntersection;

                        std::set_intersection(NewDominators.begin(), NewDominators.end(), Dominators[curBlock.Parents[k]].begin(), Dominators[curBlock.Parents[k]].end(), std::inserter(currentIntersection, currentIntersection.begin()));

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(&curBlock);

                if (NewDominators != Dominators[&curBlock])
                {
                    Dominators[&curBlock] = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }
}

void ComputeDominatorTree(CFG& CFG)
{
    CFGBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    for (CFGFunction& CFGFunc : CFG)
    {
        std::unordered_map<CFGBlock*, std::unordered_set<CFGBlock*>>& Dominators = CFG.getDominatorInfo(CFGFunc.FunctionName).Dominators;
        std::unordered_map<CFGBlock*, std::vector<CFGBlock*>>& DominatorTree = CFG.getDominatorTreeInfo(CFGFunc.FunctionName).DominatorTree;

        for (CFGBlock& block : CFGFunc.Blocks)
        {
            for (CFGBlock* dom : Dominators[&block])
            {
                if (dom == &block)
                    continue;

                size_t size = Dominators[dom].size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                DominatorTree[nearestDominator].push_back(&block);

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }
}

//ComputeWeakFrontiers is an experimental argument and has no known use cases, so avoid enabling it unless you understand what it does...

void ComputeBlockFrontiers(CFGBlock& Block, CFGDominatorInfo& DomInfo, CFGDominatorTreeInfo& DomTreeInfo, CFGFrontierInfo& FrontierInfo, bool ComputeWeakFrontiers)
{
    if (Block.TransitionNext.has_value())
    {
        if (!DomInfo.Dominators[Block.TransitionNext.value()].contains(&Block))
        {
            FrontierInfo.Frontiers[&Block].push_back(Block.TransitionNext.value());
        }
    }

    if (Block.TransitionTrue.has_value())
    {
        if (!DomInfo.Dominators[Block.TransitionTrue.value()].contains(&Block))
        {
            FrontierInfo.Frontiers[&Block].push_back(Block.TransitionTrue.value());
        }
    }

    if (Block.TransitionFalse.has_value())
    {
        if (!DomInfo.Dominators[Block.TransitionFalse.value()].contains(&Block))
        {
            FrontierInfo.Frontiers[&Block].push_back(Block.TransitionFalse.value());
        }
    }

    for (CFGBlock* domChild : DomTreeInfo.DominatorTree[&Block])
    {
        ComputeBlockFrontiers(*domChild, DomInfo, DomTreeInfo, FrontierInfo, ComputeWeakFrontiers);

        for (CFGBlock* childFrontier : FrontierInfo.Frontiers[domChild])
        {
            if ((!DomInfo.Dominators[childFrontier].contains(&Block) || childFrontier == &Block) || ComputeWeakFrontiers == true)
            {
                FrontierInfo.Frontiers[&Block].push_back(childFrontier);
            }
        }
    }
}

void ComputeFrontiers(CFG& CFG, bool ComputeWeakFrontiers)
{
    for (CFGFunction& CFGFunc : CFG)
    {
        ComputeBlockFrontiers(CFGFunc.Blocks[0], CFG.getDominatorInfo(CFGFunc.FunctionName), CFG.getDominatorTreeInfo(CFGFunc.FunctionName), CFG.getFrontierInfo(CFGFunc.FunctionName), ComputeWeakFrontiers);
    }
}

void InsertPhiNodes(CFG& CFG)
{
    for (CFGFunction& CFGFunc : CFG)
    {
        for (const auto& [var, defBlocks] : CFG.getDefBlocksInfo(CFGFunc.FunctionName).DefBlocks)
        {
            std::unordered_set<CFGBlock*> worklist = defBlocks;

            std::unordered_set<CFGBlock*> added;

            while (!worklist.empty())
            {
                auto it = worklist.begin();

                CFGBlock* block = *it;

                worklist.erase(it);

                for (CFGBlock* frontier : CFG.getFrontierInfo(CFGFunc.FunctionName).Frontiers[block])
                {
                    if (added.find(frontier) == added.end())
                    {
                        frontier->PhiNodes.push_back({var.lexeme, -1, {}});

                        added.insert(frontier);

                        worklist.insert(frontier);
                    }
                }
            }
        }
    }
}

std::unordered_map<std::string, std::pair<std::stack<int>, int>> VarStacks;

void RenameNode(Node& node, std::vector<std::string>& pushed, bool definition = false)
{
    if (node.type == NodeType::VAR)
    {
        Token& token = node.token;

        std::string originalName = token.lexeme;

        int currentId = VarStacks[originalName].second;

        VarStacks[originalName].first.push(currentId);
        VarStacks[originalName].second++;

        pushed.push_back(originalName);

        token.lexeme += std::to_string(currentId);
    }

    else if (node.type == NodeType::IDENTIFIER)
    {
        Token& token = node.token;

        if (!definition)
        {
            if (!VarStacks[token.lexeme].first.empty())
                token.lexeme += std::to_string(VarStacks[token.lexeme].first.top());
            else
                token.lexeme += "0";
        }

        else
        {
            std::string originalName = token.lexeme;

            int currentId = VarStacks[originalName].second;

            VarStacks[originalName].first.push(currentId);
            VarStacks[originalName].second++;

            pushed.push_back(originalName);

            token.lexeme += std::to_string(currentId);
        }
    }

    if (definition && node.type == NodeType::BINARY_OP)
    {
        RenameNode(node.children[1], pushed, false);

        RenameNode(node.children[0], pushed, true);
    }

    else if (definition && node.type == NodeType::EXPR)
    {
        RenameNode(node.children[0], pushed, true);
    }

    else
    {
        for (size_t i = 0; i < node.children.size(); i++)
        {
            RenameNode(node.children[i], pushed, false);
        }
    }
}

void RenameBlock(CFGBlock& Block, CFGDominatorTreeInfo& DomTreeInfo)
{
    std::vector<std::string> pushed;

    for (PhiNode& phi : Block.PhiNodes)
    {
        phi.version = VarStacks[phi.variable].second++;

        VarStacks[phi.variable].first.push(phi.version);

        pushed.push_back(phi.variable);
    }

    for (Node& s : Block.Statements)
    {
        RenameNode(s, pushed, (s.type == NodeType::VAR || s.type == NodeType::EXPR) ? true : false);
    }

    if (Block.Condition.has_value())
        RenameNode(Block.Condition.value(), pushed);

    if (Block.TransitionNext.has_value())
    {
        for (PhiNode& phi : Block.TransitionNext.value()->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block.ID, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    if (Block.TransitionTrue.has_value())
    {
        for (PhiNode& phi : Block.TransitionTrue.value()->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block.ID, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    if (Block.TransitionFalse.has_value())
    {
        for (PhiNode& phi : Block.TransitionFalse.value()->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block.ID, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    for (size_t i = 0; i < DomTreeInfo.DominatorTree[&Block].size(); i++)
        RenameBlock(*(DomTreeInfo.DominatorTree[&Block][i]), DomTreeInfo);

    for (std::string var : pushed)
    {
        VarStacks[var].first.pop();
    }
}

void RenameVariables(CFG& CFG)
{
    for (CFGFunction& CFGFunc : CFG)
    {
        RenameBlock(CFGFunc.Blocks[0], CFG.getDominatorTreeInfo(CFGFunc.FunctionName));

        VarStacks.clear();
    }
}

/*
Experimental phi insertion algorithm (abandoned)

*** Core Idea :-

Cytron's algorithm places phi nodes using dominance frontiers and discovers their incoming definitions later during the SSA renaming DFS

This experiment attempted to combine both steps by propagating definition sources during phi placement itself using "weak dominance frontiers" (dominance frontiers with the secondary filtering constraint removed)

The goal was to compute both:
    -- phi placement
    -- candidate phi arguments

in a single iterative propagation...

*** Why it was bound to fail :-

"weak dominance frontiers" naturally overpropagate definition sources to every downstream merge point. Eliminating those "dead" phi arguments ultimately required determining which definitions actually reach each join point after trying many other approaches

And that problem is literally the classic reaching definitions problem...

Cytron's renaming DFS already performs this reasoning implicitly while assigning SSA names, making the additional propagation performed here largely redundant. Any exact pruning strategy ultimately reconstructed a worse reaching definitions pass

Decided to keep the implementation as a research artifact...
*/

/*
void InsertPhiNodes(CFG &CFG)
{
    for (auto &CFGFunc : CFG)
    {
        bool newPhiAdded = true;

        while (newPhiAdded)
        {
            newPhiAdded = false;

            for (auto &block : CFGFunc->Blocks)
            {
                std::unordered_set<Token> varDefs;

                for (auto &statement : block->Statements)
                {
                    if (statement->type == NodeType::VAR)
                    {
                        varDefs.insert(statement->token);
                    }

                    else if (statement->type == NodeType::EXPR && statement->children[0]->type == NodeType::BINARY_OP)
                    {
                        varDefs.insert(statement->children[0]->children[0]->token);
                    }
                }

                for (auto &frontier : block->Frontiers)
                {
                    frontier->PhiPlacementInfo.push_back(VarDefInfo{block.get(), varDefs});
                }
            }

            for (auto &block : CFGFunc->Blocks)
            {
                std::unordered_map<Token, std::pair<int, std::vector<CFGBlock *>>> Counter;

                for (VarDefInfo varDefInfo : block->PhiPlacementInfo)
                {
                    for (auto &var : varDefInfo.vardefs)
                    {
                        Counter[var].first++;
                        Counter[var].second.push_back(varDefInfo.source);
                    }
                }

                block->PhiPlacementInfo.clear();

                for (auto &var : Counter)
                {
                    if (Counter[var.first].first >= 2)
                    {
                        if (!block->ExisitingPhiNodes.contains(var.first))
                        {
                            std::unique_ptr<Node> phiNode = std::make_unique<Node>(Node{NodeType::EXPR, {}, {}});

                            phinode.children.push_back(std::make_unique<Node>(Node{NodeType::BINARY_OP, Token{TokenType::EQUAL, "="}, {}}));

                            phinode.children[0]->children.push_back(std::make_unique<Node>(Node{NodeType::IDENTIFIER, var.first, {}}));

                            phinode.children[0]->children.push_back(std::make_unique<Node>(Node{NodeType::CALL, Token{TokenType::IDENTIFIER, "PHI"}, {}}));

                            for (CFGBlock *src : Counter[var.first].second)
                            {
                                phinode.children[0]->children[1]->children.push_back(std::make_unique<Node>(Node{NodeType::IDENTIFIER, var.first, {}, src}));
                            }

                            block->Statements.insert(block->Statements.begin(), std::move(phiNode));

                            block->ExisitingPhiNodes.insert(var.first);

                            newPhiAdded = true;
                        }
                    }
                }
            }
        }
    }
}
*/
