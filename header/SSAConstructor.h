#include "CFGBuilder.h"
#include <memory>

void ComputeDominators(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void ComputeDominatorTree(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void ComputeFrontiers(CFGBlock *Block);
