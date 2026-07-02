#pragma once

#include "CFGBuilder.h"
#include <memory>

void ComputeDominators(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void ComputeDominatorTree(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void ComputeFrontiers(CFGBlock *Block, bool ComputeWeakFrontiers = false);

void InsertPhiNodes(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void RenameVariables(std::vector<std::unique_ptr<CFGFunction>> &CFG);
