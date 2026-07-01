#include "SSAConstructor.h"
#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <utility>

void ComputeDominators(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (auto &FuncPtr : CFG)
    {
        if (!FuncPtr || FuncPtr->Blocks.empty())
            continue;

        auto &Blocks = FuncPtr->Blocks;

        CFGBlock *entryBlock = Blocks[0].get();

        std::set<CFGBlock *> universalSet;

        for (const auto &blockUniquePtr : Blocks)
        {
            universalSet.insert(blockUniquePtr.get());
        }

        entryBlock->Dominators = {entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            Blocks[j]->Dominators = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                CFGBlock *curBlock = Blocks[j].get();

                if (curBlock == entryBlock)
                    continue;

                std::set<CFGBlock *> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = curBlock->Parents[0]->Dominators;

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::set<CFGBlock *> currentIntersection;
                        std::set_intersection(NewDominators.begin(), NewDominators.end(), curBlock->Parents[k]->Dominators.begin(), curBlock->Parents[k]->Dominators.end(), std::inserter(currentIntersection, currentIntersection.begin()));

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(curBlock);

                if (NewDominators != curBlock->Dominators)
                {
                    curBlock->Dominators = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }
}

void ComputeDominatorTree(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    CFGBlock *nearestDominator = nullptr;
    size_t maxSize = 0;

    for (auto &CFGFunc : CFG)
    {
        for (auto &block : CFGFunc->Blocks)
        {
            for (CFGBlock *dom : block->Dominators)
            {
                if (dom == block.get())
                    continue;

                size_t size = dom->Dominators.size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                nearestDominator->DominatorTreeChildren.push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }
}

//ComputeWeakFrontiers is an experimental argument and has no known use cases, so avoid enabling it unless you understand what it does...

void ComputeFrontiers(CFGBlock *Block, bool ComputeWeakFrontiers)
{
    if (Block->TransitionNext != nullptr)
    {
        if (!Block->TransitionNext->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionNext);
        }
    }

    if (Block->TransitionTrue != nullptr)
    {
        if (!Block->TransitionTrue->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionTrue);
        }
    }

    if (Block->TransitionFalse != nullptr)
    {
        if (!Block->TransitionFalse->Dominators.contains(Block))
        {
            Block->Frontiers.insert(Block->TransitionFalse);
        }
    }

    for (auto domChild : Block->DominatorTreeChildren)
    {
        ComputeFrontiers(domChild, ComputeWeakFrontiers);

        for (auto childFrontier : domChild->Frontiers)
        {
            if ((!childFrontier->Dominators.contains(Block) || childFrontier == Block) || ComputeWeakFrontiers == true)
            {
                Block->Frontiers.insert(childFrontier);
            }
        }
    }
}

void InsertPhiNodes(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (auto &CFGFunc : CFG)
    {
        for (const auto &[var, defBlocks] : CFGFunc->DefBlocks)
        {
            std::set<CFGBlock *> worklist = defBlocks;

            std::set<CFGBlock *> added;

            while (!worklist.empty())
            {
                auto it = worklist.begin();
                CFGBlock *block = *it;
                worklist.erase(it);

                for (CFGBlock *frontier : block->Frontiers)
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

std::map<std::string, std::pair<std::stack<int>, int>> VarStacks;

void RenameNode(Node *node, std::vector<std::string> &pushed, bool definition = false)
{
    if (node->type == NodeType::VAR)
    {
        Token &token = node->token;

        std::string originalName = token.lexeme;

        int currentId = VarStacks[originalName].second;

        VarStacks[originalName].first.push(currentId);
        VarStacks[originalName].second++;

        pushed.push_back(originalName);

        token.lexeme += std::to_string(currentId);
    }

    else if (node->type == NodeType::IDENTIFIER)
    {
        Token &token = node->token;

        if (!definition)
        {
            if (!VarStacks[token.lexeme].first.empty())
                token.lexeme += std::to_string(VarStacks[token.lexeme].first.top());
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

    if (definition && node->type == NodeType::BINARY_OP)
    {
        RenameNode(node->children[1].get(), pushed, false);

        RenameNode(node->children[0].get(), pushed, true);
    }
    else
    {
        for (size_t i = 0; i < node->children.size(); i++)
        {
            RenameNode(node->children[i].get(), pushed, definition);
        }
    }
}

void RenameBlock(CFGBlock *Block)
{
    std::vector<std::string> pushed;

    for (auto &phi : Block->PhiNodes)
    {
        phi.version = VarStacks[phi.variable].second++;

        VarStacks[phi.variable].first.push(phi.version);

        pushed.push_back(phi.variable);
    }

    for (auto &s : Block->Statements)
    {
        RenameNode(s.get(), pushed, s->type == NodeType::VAR || s->type == NodeType::EXPR ? true : false);
    }

    if (Block->Condition != nullptr)
        RenameNode(Block->Condition.get(), pushed);

    if (Block->TransitionNext != nullptr)
    {
        for (auto &phi : Block->TransitionNext->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    if (Block->TransitionTrue != nullptr)
    {
        for (auto &phi : Block->TransitionTrue->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    if (Block->TransitionFalse != nullptr)
    {
        for (auto &phi : Block->TransitionFalse->PhiNodes)
        {
            if (!VarStacks[phi.variable].first.empty())
                phi.arguments.push_back(PhiArgument{Block, phi.variable + std::to_string(VarStacks[phi.variable].first.top())});
        }
    }

    for (size_t i = 0; i < Block->DominatorTreeChildren.size(); i++)
        RenameBlock(Block->DominatorTreeChildren[i]);

    for (auto var : pushed)
    {
        VarStacks[var].first.pop();
    }
}

void RenameVariables(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (auto &CFGFunc : CFG)
    {
        RenameBlock(CFGFunc->Blocks[0].get());

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
void InsertPhiNodes(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    for (auto &CFGFunc : CFG)
    {
        bool newPhiAdded = true;

        while (newPhiAdded)
        {
            newPhiAdded = false;

            for (auto &block : CFGFunc->Blocks)
            {
                std::set<Token> varDefs;

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
                std::map<Token, std::pair<int, std::vector<CFGBlock *>>> Counter;

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

                            phiNode->children.push_back(std::make_unique<Node>(Node{NodeType::BINARY_OP, Token{TokenType::EQUAL, "="}, {}}));

                            phiNode->children[0]->children.push_back(std::make_unique<Node>(Node{NodeType::IDENTIFIER, var.first, {}}));

                            phiNode->children[0]->children.push_back(std::make_unique<Node>(Node{NodeType::CALL, Token{TokenType::IDENTIFIER, "PHI"}, {}}));

                            for (CFGBlock *src : Counter[var.first].second)
                            {
                                phiNode->children[0]->children[1]->children.push_back(std::make_unique<Node>(Node{NodeType::IDENTIFIER, var.first, {}, src}));
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
