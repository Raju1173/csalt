#pragma once

#include "CFGBuilder.h"

void ComputeDominators(CFG& CFG);

void ComputeDominatorTree(CFG& CFG);

void ComputeBlockFrontiers(CFGBlock& Block, CFGDominatorInfo& DomInfo, CFGDominatorTreeInfo& DomTreeInfo, CFGFrontierInfo& FrontierInfo, bool ComputeWeakFrontiers);

void ComputeFrontiers(CFG& CFG, bool ComputeWeakFrontiers = false);

void InsertPhiNodes(CFG& CFG);

void RenameVariables(CFG& CFG);
