#include "PeepholeOptimizations.h"

bool PeepholeOptimize(MIR& MIR)
{
    bool changed = false;

    for (auto& MIRFunc : MIR)
    {
        for (auto& MIRBlock : MIRFunc->Blocks)
        {
            for (auto& inst : MIRBlock->Instructions)
            {
                //
            }
        }
    }

    return changed;
}
