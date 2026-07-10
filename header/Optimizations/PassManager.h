#include "TACGenerator.h"
#include <string>
#include <vector>

struct Pass
{
    std::string Name;

    bool Enabled = true;

    bool PrintDiff = false;

    bool (*Run)(TAC&);
};

struct PassGroup
{
    bool IterateToFixedPoint = false;
    std::vector<Pass> Passes;
};

class PassManager
{
public:
    std::vector<PassGroup> OptimizationPipeline;

    PassManager(std::vector<PassGroup> OptimizationPipeline) : OptimizationPipeline(OptimizationPipeline){};

    void RunOptimizations(TAC& TAC);
};
