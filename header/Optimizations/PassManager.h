#include "MIRGenerator.h"
#include "TACGenerator.h"
#include <optional>
#include <string>
#include <vector>

union PassFuncUnion
{
    void (*RunTAC)(TAC&);
    bool (*RunTACIter)(TAC&);
    void (*RunMIR)(MIR&);
    bool (*RunMIRIter)(MIR&);
};

struct Pass
{
    std::string Name;

    bool Enabled = true;

    bool PrintDiff = false;

    PassFuncUnion Func;
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

    void RunOptimizations(TAC* TAC, MIR* MIR);
};
